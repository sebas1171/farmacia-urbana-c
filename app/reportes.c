#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <libpq-fe.h>

#include "reportes.h"

/* Verifica que `fecha` tenga el formato ISO "YYYY-MM-DD":
   - 10 caracteres exactos
   - dígitos en las posiciones 0-3, 5-6 y 8-9
   - guiones en las posiciones 4 y 7 */
static int fechaValida(const char *fecha) {
    if (fecha == NULL || strlen(fecha) != 10) {
        return 0;
    }
    if (fecha[4] != '-' || fecha[7] != '-') {
        return 0;
    }
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            continue;
        }
        if (fecha[i] < '0' || fecha[i] > '9') {
            return 0;
        }
    }
    return 1;
}

/* Valida formato YYYY-MM-DD y que la fecha exista en el calendario real
   (mes entre 01 y 12 y dia valido para el ano, incluido Febrero en anos bisiestos). */
static int fechaValidaCalendario(const char *fecha) {
    if (!fechaValida(fecha)) {
        return 0;
    }
    int anio = (fecha[0] - '0') * 1000 + (fecha[1] - '0') * 100 + (fecha[2] - '0') * 10 + (fecha[3] - '0');
    int mes = (fecha[5] - '0') * 10 + (fecha[6] - '0');
    int dia = (fecha[8] - '0') * 10 + (fecha[9] - '0');
    if (mes < 1 || mes > 12) {
        return 0;
    }
    static const int diasPorMes[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int max_dia = diasPorMes[mes];
    if (mes == 2 && ((anio % 4 == 0 && anio % 100 != 0) || (anio % 400 == 0))) {
        max_dia = 29;
    }
    if (dia < 1 || dia > max_dia) {
        return 0;
    }
    return 1;
}

/* Valida el ID de vendedor: solo digitos positivos. El ID vacio ("") significa
   "sin filtro" (se muestran ventas de todos los vendedores). */
static int idVendedorValido(const char *id) {
    if (id == NULL) {
        return 0;
    }
    if (strlen(id) == 0) {
        return 0; /* vacio: sin vendedor (aceptado como "todos") */
    }
    for (int i = 0; id[i] != '\0'; i++) {
        if (id[i] < '0' || id[i] > '9') {
            return 0;
        }
    }
    errno = 0;
    long valor = strtol(id, NULL, 10);
    return errno != ERANGE && valor > 0 && valor <= INT_MAX;
}

/* Valida el DNI del cliente: exactamente 8 digitos. */
static int dniClienteValido(const char *dni) {
    if (dni == NULL || strlen(dni) != 8) return 0;
    for (int i = 0; i < 8; i++) {
        if (!isdigit((unsigned char)dni[i])) return 0;
    }
    return 1;
}

/* Agrega una condicion al WHERE dinamico con el conector AND cuando hace falta. */
static void agregarCondicionSQL(char *where, size_t tam, const char *condicion) {
    if (where == NULL || condicion == NULL || tam == 0) {
        return;
    }
    size_t usado = strlen(where);
    if (usado >= tam - 1) {
        return;
    }
    if (usado > 0) {
        snprintf(where + usado, tam - usado, " AND ");
        usado = strlen(where);
    }
    snprintf(where + usado, tam - usado, "%s", condicion);
}

int reporteStockMinimo(const Producto inventario[], int total, int umbral) {
    if (inventario == NULL || total <= 0) {
        return 0;
    }

    int impresos = 0;
    for (int i = 0; i < total; i++) {
        if (inventario[i].stock < (double)umbral) {
            printf("%d;%s;%.2f;%s\n",
                inventario[i].id,
                inventario[i].nombre,
                inventario[i].stock,
                inventario[i].fecha_vencimiento);
            impresos++;
        }
    }
    return impresos;
}

int reporteVencimientos(const Producto inventario[], int total, const char *fecha_limite) {
    if (inventario == NULL || total <= 0 || !fechaValida(fecha_limite)) {
        return 0;
    }

    int impresos = 0;
    for (int i = 0; i < total; i++) {
        if (!fechaValida(inventario[i].fecha_vencimiento)) {
            continue;  /* fecha malformada: se omite para no comparar en falso */
        }
        if (strcmp(inventario[i].fecha_vencimiento, fecha_limite) <= 0) {
            printf("%d;%s;%.2f;%s\n",
                inventario[i].id,
                inventario[i].nombre,
                inventario[i].stock,
                inventario[i].fecha_vencimiento);
            impresos++;
        }
    }
    return impresos;
}

/* --- PostgreSQL (libpq): historial de ventas por periodo --- */

/* Conexión PostgreSQL (libpq). Se abre perezosamente con la cadena de
   conexión de la variable de entorno FARMACIA_DB_CONN. */
static PGconn *conexion = NULL;

/* Devuelve la conexión activa o NULL si no está disponible. */
static PGconn *getConexion() {
    const char *conn_str = getenv("FARMACIA_DB_CONN");

    if (conn_str == NULL || conn_str[0] == '\0') {
        printf("ERROR;FARMACIA_DB_CONN_no_definida\n");
        return NULL;
    }

    if (conexion == NULL) {
        conexion = PQconnectdb(conn_str);
    }

    if (conexion == NULL || PQstatus(conexion) != CONNECTION_OK) {
        if (conexion != NULL) {
            fprintf(stderr, "ERROR;detalle_conexion: %s\n", PQerrorMessage(conexion));
            PQfinish(conexion);
            conexion = NULL;
        }
        printf("ERROR;conexion_farmacia_fallida\n");
        return NULL;
    }

    return conexion;
}

int reporteHistorialVentas(const char *fecha_inicio, const char *fecha_fin,
                           const char *id_vendedor, const char *dni_cliente,
                           int id_sucursal) {
    if (id_sucursal <= 0) {
        printf("ERROR;Sucursal_Invalida\n");
        return -1;
    }

    int usarFechaInicio = fecha_inicio != NULL && fecha_inicio[0] != '\0';
    int usarFechaFin = fecha_fin != NULL && fecha_fin[0] != '\0';

    /* Fechas vacias = reporte abierto: se omiten del WHERE y se devuelven
       todas las ventas registradas. */
    if (usarFechaInicio && !fechaValidaCalendario(fecha_inicio)) {
        printf("ERROR;Fecha_Invalida\n");
        return -1;
    }
    if (usarFechaFin && !fechaValidaCalendario(fecha_fin)) {
        printf("ERROR;Fecha_Invalida\n");
        return -1;
    }
    if (usarFechaInicio && usarFechaFin && strcmp(fecha_inicio, fecha_fin) > 0) {
        printf("ERROR;Rango_Fechas_Invalido\n");
        return -1;
    }

    int usarFiltroVendedor = id_vendedor != NULL && strlen(id_vendedor) > 0;
    int usarFiltroCliente = dni_cliente != NULL && strlen(dni_cliente) > 0;

    if (usarFiltroVendedor && !idVendedorValido(id_vendedor)) {
        printf("ERROR;ID_Vendedor_Invalido\n");
        return -1;
    }
    if (usarFiltroCliente && !dniClienteValido(dni_cliente)) {
        printf("ERROR;DNI_Cliente_Invalido\n");
        return -1;
    }

    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    char sql[1024];
    char where[512] = "";
    char condicion[160];
    Oid paramTypes[5] = {0, 0, 0, 0, 0};
    const char *paramValues[5] = {NULL, NULL, NULL, NULL, NULL};
    int paramLengths[5] = {0, 0, 0, 0, 0};
    const int paramFormats[5] = {0, 0, 0, 0, 0};
    int nparams = 0;

    char sucursal_txt[16];
    snprintf(sucursal_txt, sizeof(sucursal_txt), "%d", id_sucursal);

    /* El historial SIEMPRE queda acotado a la sucursal de la sesión. */
    nparams++;
    snprintf(condicion, sizeof(condicion),
             "v.id_sucursal = $%d::integer", nparams);
    agregarCondicionSQL(where, sizeof(where), condicion);
    paramTypes[nparams - 1] = 23;
    paramValues[nparams - 1] = sucursal_txt;
    paramLengths[nparams - 1] = (int)strlen(sucursal_txt);

    if (usarFechaInicio) {
        nparams++;
        snprintf(condicion, sizeof(condicion),
                 "v.fecha_hora >= CAST($%d AS date)", nparams);
        agregarCondicionSQL(where, sizeof(where), condicion);
        paramTypes[nparams - 1] = 25;
        paramValues[nparams - 1] = fecha_inicio;
        paramLengths[nparams - 1] = (int)strlen(fecha_inicio);
    }

    if (usarFechaFin) {
        nparams++;
        snprintf(condicion, sizeof(condicion),
                 "v.fecha_hora < (CAST($%d AS date) + INTERVAL '1 day')", nparams);
        agregarCondicionSQL(where, sizeof(where), condicion);
        paramTypes[nparams - 1] = 25;
        paramValues[nparams - 1] = fecha_fin;
        paramLengths[nparams - 1] = (int)strlen(fecha_fin);
    }

    if (usarFiltroVendedor) {
        nparams++;
        snprintf(condicion, sizeof(condicion),
                 "v.id_vendedor = $%d::integer", nparams);
        agregarCondicionSQL(where, sizeof(where), condicion);
        paramTypes[nparams - 1] = 23;
        paramValues[nparams - 1] = id_vendedor;
        paramLengths[nparams - 1] = (int)strlen(id_vendedor);
    }

    if (usarFiltroCliente) {
        nparams++;
        snprintf(condicion, sizeof(condicion),
                 "v.dni_cliente = $%d", nparams);
        agregarCondicionSQL(where, sizeof(where), condicion);
        paramTypes[nparams - 1] = 25;
        paramValues[nparams - 1] = dni_cliente;
        paramLengths[nparams - 1] = (int)strlen(dni_cliente);
    }

    /* Historial a nivel de venta (sin joins): una fila por boleta/venta. */
    snprintf(sql, sizeof(sql),
        "SELECT v.id, v.dni_cliente, "
        "TO_CHAR(v.fecha_hora, 'YYYY-MM-DD HH24:MI:SS.MS'), v.total "
        "FROM venta v%s%s "
        "ORDER BY v.fecha_hora DESC, v.id DESC",
        where[0] != '\0' ? " WHERE " : "", where);

    /* Si no hay filtros (fechas vacías y sin vendedor/cliente), libpq debe
       recibir NULL en los arreglos de parámetros: el WHERE se omite y se
       devuelven todas las ventas registradas. */
    PGresult *res = PQexecParams(
        conn, sql, nparams,
        nparams > 0 ? paramTypes : NULL,
        nparams > 0 ? paramValues : NULL,
        nparams > 0 ? paramLengths : NULL,
        nparams > 0 ? paramFormats : NULL,
        0);

    if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "ERROR;detalle_consulta_historial: %s\n",
                res != NULL ? PQerrorMessage(conn) : "sin resultado");
        printf("ERROR;consulta_historial_fallida\n");
        if (res != NULL) PQclear(res);
        PQfinish(conn);
        conexion = NULL;
        return -1;
    }

    int filas = PQntuples(res);
    if (filas == 0) {
        printf("OK;SIN_RESULTADOS\n");
        PQclear(res);
        PQfinish(conn);
        conexion = NULL;
        return 0;
    }

    int impresos = 0;
    for (int i = 0; i < filas; i++) {
        const char *id_venta = PQgetvalue(res, i, 0);
        const char *dni_cliente = PQgetvalue(res, i, 1);
        const char *fecha = PQgetvalue(res, i, 2);
        const char *total = PQgetvalue(res, i, 3);

        printf("OK;FILA;%s;%s;%s;%.2f\n",
               id_venta, dni_cliente, fecha, strtod(total, NULL));
        impresos++;
    }

    PQclear(res);
    PQfinish(conn);
    conexion = NULL;
    return impresos;
}

/* --- Motor de alertas (stock <= 10 y productos que vencen en menos de 30 dias) --- */

int reporteAlertas(int id_sucursal) {
    if (id_sucursal <= 0) {
        printf("ERROR;Sucursal_Invalida\n");
        return -1;
    }

    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    char sucursal_txt[16];
    snprintf(sucursal_txt, sizeof(sucursal_txt), "%d", id_sucursal);
    const Oid tipos[] = { 23 };
    const char *valores[] = { sucursal_txt };

    char sql_stock[256];
    snprintf(sql_stock, sizeof(sql_stock),
             "SELECT COUNT(*) FROM producto "
             "WHERE stock <= %d AND id_sucursal = $1",
             STOCK_MINIMO_DEFAULT);
    PGresult *res = PQexecParams(conn, sql_stock, 1, tipos, valores,
                                 NULL, NULL, 0);
    if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "ERROR;detalle_consulta_alertas: %s\n", res != NULL ? PQerrorMessage(conn) : "sin resultado");
        printf("ERROR;consulta_alertas_fallida\n");
        if (res != NULL) {
            PQclear(res);
        }
        PQfinish(conn);
        conexion = NULL;
        return -1;
    }
    int bajo_stock = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    /* CURRENT_DATE depende de la zona horaria del contenedor (normalmente
       UTC). Se usa la fecha local de Perú para que el conteo de vencimientos
       coincida con la fecha que ve el usuario. */
    const char *sql_venc =
        "SELECT COUNT(*) FROM producto "
        "WHERE fecha_vencimiento >= "
        "(CURRENT_TIMESTAMP AT TIME ZONE 'America/Lima')::date "
        "AND fecha_vencimiento < "
        "((CURRENT_TIMESTAMP AT TIME ZONE 'America/Lima')::date "
        "+ INTERVAL '30 days') "
        "AND id_sucursal = $1";
    res = PQexecParams(conn, sql_venc, 1, tipos, valores, NULL, NULL, 0);
    if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "ERROR;detalle_consulta_alertas: %s\n", res != NULL ? PQerrorMessage(conn) : "sin resultado");
        printf("ERROR;consulta_alertas_fallida\n");
        if (res != NULL) {
            PQclear(res);
        }
        PQfinish(conn);
        conexion = NULL;
        return -1;
    }
    int por_vencer = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    PQfinish(conn);
    conexion = NULL;

    printf("OK;ALERTAS;BAJO_STOCK=%d;POR_VENCER=%d\n", bajo_stock, por_vencer);
    return 0;
}

/* --- Metrica: producto mas vendido por unidades en el periodo --- */

int reporteProductoMasVendido(const char *fecha_inicio, const char *fecha_fin,
                              int id_sucursal) {
    if (id_sucursal <= 0) {
        printf("ERROR;Sucursal_Invalida\n");
        return -1;
    }

    int usarInicio = fecha_inicio != NULL && fecha_inicio[0] != '\0';
    int usarFin = fecha_fin != NULL && fecha_fin[0] != '\0';

    /* Fechas vacías = reporte abierto (todas las ventas). */
    if (usarInicio && !fechaValidaCalendario(fecha_inicio)) {
        printf("ERROR;Fecha_Invalida\n");
        return -1;
    }
    if (usarFin && !fechaValidaCalendario(fecha_fin)) {
        printf("ERROR;Fecha_Invalida\n");
        return -1;
    }
    if (usarInicio && usarFin && strcmp(fecha_inicio, fecha_fin) > 0) {
        printf("ERROR;Rango_Fechas_Invalido\n");
        return -1;
    }

    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    char where[256] = "";
    Oid paramTypes[3] = { 23, 25, 25 };
    const char *paramValues[3] = { NULL, NULL, NULL };
    int paramLengths[3] = { 0, 0, 0 };
    const int paramFormats[3] = { 0, 0, 0 };
    int nparams = 0;

    char sucursal_txt[16];
    snprintf(sucursal_txt, sizeof(sucursal_txt), "%d", id_sucursal);

    /* Filtro obligatorio por sucursal: nunca se mezclan ventas de locales. */
    nparams++;
    snprintf(where + strlen(where), sizeof(where) - strlen(where),
             "WHERE v.id_sucursal = $%d::integer", nparams);
    paramValues[nparams - 1] = sucursal_txt;
    paramLengths[nparams - 1] = (int)strlen(sucursal_txt);

    if (usarInicio) {
        nparams++;
        snprintf(where + strlen(where), sizeof(where) - strlen(where),
                 "%sv.fecha_hora >= CAST($%d AS date)",
                 where[0] != '\0' ? " AND " : "WHERE ", nparams);
        paramValues[nparams - 1] = fecha_inicio;
        paramLengths[nparams - 1] = (int)strlen(fecha_inicio);
    }
    if (usarFin) {
        nparams++;
        snprintf(where + strlen(where), sizeof(where) - strlen(where),
                 "%sv.fecha_hora < (CAST($%d AS date) + INTERVAL '1 day')",
                 where[0] != '\0' ? " AND " : "WHERE ", nparams);
        paramValues[nparams - 1] = fecha_fin;
        paramLengths[nparams - 1] = (int)strlen(fecha_fin);
    }

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT p.codigo_producto, p.nombre, SUM(dv.cantidad) AS total "
             "FROM venta v "
             "INNER JOIN detalle_venta dv ON dv.id_venta = v.id "
             "INNER JOIN producto p ON p.codigo_producto = dv.codigo_producto "
             "%s "
             "GROUP BY p.codigo_producto, p.nombre "
             "ORDER BY total DESC, p.codigo_producto ASC "
             "LIMIT 1",
             where);

    PGresult *res = PQexecParams(
        conn, sql, nparams,
        nparams > 0 ? paramTypes : NULL,
        nparams > 0 ? paramValues : NULL,
        nparams > 0 ? paramLengths : NULL,
        nparams > 0 ? paramFormats : NULL,
        0);
    if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "ERROR;detalle_consulta_producto_top: %s\n", res != NULL ? PQerrorMessage(conn) : "sin resultado");
        printf("ERROR;consulta_producto_top_fallida\n");
        if (res != NULL) {
            PQclear(res);
        }
        PQfinish(conn);
        conexion = NULL;
        return -1;
    }

    int filas = PQntuples(res);
    if (filas == 0) {
        printf("OK;SIN_RESULTADOS\n");
        PQclear(res);
        PQfinish(conn);
        conexion = NULL;
        return 0;
    }

    printf("OK;PRODUCTO_MAS_VENDIDO;%s;%s;%.2f\n",
           PQgetvalue(res, 0, 0), PQgetvalue(res, 0, 1),
           strtod(PQgetvalue(res, 0, 2), NULL));
    PQclear(res);
    PQfinish(conn);
    conexion = NULL;
    return 0;
}

/* --- Metrica: ingresos totales por periodo (SUM con COALESCE) --- */

int reporteIngresosTotales(const char *fecha_inicio, const char *fecha_fin,
                           int id_sucursal) {
    if (id_sucursal <= 0) {
        printf("ERROR;Sucursal_Invalida\n");
        return -1;
    }

    int usarInicio = fecha_inicio != NULL && fecha_inicio[0] != '\0';
    int usarFin = fecha_fin != NULL && fecha_fin[0] != '\0';

    /* Fechas vacías = reporte abierto (todas las ventas). */
    if (usarInicio && !fechaValidaCalendario(fecha_inicio)) {
        printf("ERROR;Fecha_Invalida\n");
        return -1;
    }
    if (usarFin && !fechaValidaCalendario(fecha_fin)) {
        printf("ERROR;Fecha_Invalida\n");
        return -1;
    }
    if (usarInicio && usarFin && strcmp(fecha_inicio, fecha_fin) > 0) {
        printf("ERROR;Rango_Fechas_Invalido\n");
        return -1;
    }

    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    char where[256] = "";
    Oid paramTypes[3] = { 23, 25, 25 };
    const char *paramValues[3] = { NULL, NULL, NULL };
    int paramLengths[3] = { 0, 0, 0 };
    const int paramFormats[3] = { 0, 0, 0 };
    int nparams = 0;

    char sucursal_txt[16];
    snprintf(sucursal_txt, sizeof(sucursal_txt), "%d", id_sucursal);

    /* Filtro obligatorio por sucursal: nunca se mezclan ventas de locales. */
    nparams++;
    snprintf(where + strlen(where), sizeof(where) - strlen(where),
             "WHERE v.id_sucursal = $%d::integer", nparams);
    paramValues[nparams - 1] = sucursal_txt;
    paramLengths[nparams - 1] = (int)strlen(sucursal_txt);

    if (usarInicio) {
        nparams++;
        snprintf(where + strlen(where), sizeof(where) - strlen(where),
                 "%sv.fecha_hora >= CAST($%d AS date)",
                 where[0] != '\0' ? " AND " : "WHERE ", nparams);
        paramValues[nparams - 1] = fecha_inicio;
        paramLengths[nparams - 1] = (int)strlen(fecha_inicio);
    }
    if (usarFin) {
        nparams++;
        snprintf(where + strlen(where), sizeof(where) - strlen(where),
                 "%sv.fecha_hora < (CAST($%d AS date) + INTERVAL '1 day')",
                 where[0] != '\0' ? " AND " : "WHERE ", nparams);
        paramValues[nparams - 1] = fecha_fin;
        paramLengths[nparams - 1] = (int)strlen(fecha_fin);
    }

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT COALESCE(SUM(dv.subtotal), 0) AS total "
             "FROM venta v "
             "INNER JOIN detalle_venta dv ON dv.id_venta = v.id "
             "%s",
             where);

    PGresult *res = PQexecParams(
        conn, sql, nparams,
        nparams > 0 ? paramTypes : NULL,
        nparams > 0 ? paramValues : NULL,
        nparams > 0 ? paramLengths : NULL,
        nparams > 0 ? paramFormats : NULL,
        0);
    if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "ERROR;detalle_consulta_ingresos: %s\n", res != NULL ? PQerrorMessage(conn) : "sin resultado");
        printf("ERROR;consulta_ingresos_fallida\n");
        if (res != NULL) {
            PQclear(res);
        }
        PQfinish(conn);
        conexion = NULL;
        return -1;
    }

    /* COALESCE garantiza una fila con el total (0 si no hay ventas). */
    printf("OK;INGRESOS_TOTALES;%.2f\n", strtod(PQgetvalue(res, 0, 0), NULL));
    PQclear(res);
    PQfinish(conn);
    conexion = NULL;
    return 0;
}

/* --- Listado de productos con stock critico desde PostgreSQL --- */

int reporteAlertasStock(int id_sucursal) {
    if (id_sucursal <= 0) {
        printf("ERROR;Sucursal_Invalida\n");
        return -1;
    }

    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    char sucursal_txt[16];
    snprintf(sucursal_txt, sizeof(sucursal_txt), "%d", id_sucursal);
    const Oid tipos[] = { 23 };
    const char *valores[] = { sucursal_txt };

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT codigo_producto, nombre, stock "
             "FROM producto WHERE stock <= %d AND id_sucursal = $1 "
             "ORDER BY stock ASC, nombre ASC",
             STOCK_MINIMO_DEFAULT);

    PGresult *res = PQexecParams(conn, sql, 1, tipos, valores,
                                 NULL, NULL, 0);

    if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "ERROR;detalle_alertas_stock: %s\n",
                res != NULL ? PQerrorMessage(conn) : "sin resultado");
        printf("ERROR;consulta_alertas_stock_fallida\n");
        if (res != NULL) PQclear(res);
        PQfinish(conn);
        conexion = NULL;
        return -1;
    }

    int filas = PQntuples(res);

    if (filas == 0) {
        printf("OK;SIN_RESULTADOS\n");
    } else {
        for (int i = 0; i < filas; i++) {
            printf("OK;STOCK;%s;%s;%.2f\n",
                   PQgetvalue(res, i, 0),
                   PQgetvalue(res, i, 1),
                   strtod(PQgetvalue(res, i, 2), NULL));
        }
    }

    PQclear(res);
    PQfinish(conn);
    conexion = NULL;
    return 0;
}

/* --- Listado de productos proximos a vencer desde PostgreSQL --- */

int reporteAlertasVencimientos(int id_sucursal) {
    if (id_sucursal <= 0) {
        printf("ERROR;Sucursal_Invalida\n");
        return -1;
    }

    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    char sucursal_txt[16];
    snprintf(sucursal_txt, sizeof(sucursal_txt), "%d", id_sucursal);
    const Oid tipos[] = { 23 };
    const char *valores[] = { sucursal_txt };

    const char *sql =
        "SELECT codigo_producto, nombre, TO_CHAR(fecha_vencimiento, 'YYYY-MM-DD') "
        "FROM producto "
        "WHERE fecha_vencimiento >= "
        "(CURRENT_TIMESTAMP AT TIME ZONE 'America/Lima')::date "
        "AND fecha_vencimiento < "
        "((CURRENT_TIMESTAMP AT TIME ZONE 'America/Lima')::date "
        "+ INTERVAL '30 days') "
        "AND id_sucursal = $1 "
        "ORDER BY fecha_vencimiento ASC, nombre ASC";

    PGresult *res = PQexecParams(conn, sql, 1, tipos, valores,
                                 NULL, NULL, 0);

    if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "ERROR;detalle_alertas_vencimientos: %s\n",
                res != NULL ? PQerrorMessage(conn) : "sin resultado");
        printf("ERROR;consulta_alertas_vencimientos_fallida\n");
        if (res != NULL) PQclear(res);
        PQfinish(conn);
        conexion = NULL;
        return -1;
    }

    int filas = PQntuples(res);

    if (filas == 0) {
        printf("OK;SIN_RESULTADOS\n");
    } else {
        for (int i = 0; i < filas; i++) {
            printf("OK;VENCIMIENTO;%s;%s;%s\n",
                   PQgetvalue(res, i, 0),
                   PQgetvalue(res, i, 1),
                   PQgetvalue(res, i, 2));
        }
    }

    PQclear(res);
    PQfinish(conn);
    conexion = NULL;
    return 0;
}
