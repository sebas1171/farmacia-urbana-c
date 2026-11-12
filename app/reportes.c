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

int reporteStockMinimo(const Producto inventario[], int total, int umbral) {
    if (inventario == NULL || total <= 0) {
        return 0;
    }

    int impresos = 0;
    for (int i = 0; i < total; i++) {
        if (inventario[i].stock < umbral) {
            printf("%d;%s;%d;%s\n",
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
            printf("%d;%s;%d;%s\n",
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
                           const char *id_vendedor, const char *dni_cliente) {
    if (fecha_inicio == NULL || fecha_fin == NULL ||
        !fechaValidaCalendario(fecha_inicio) || !fechaValidaCalendario(fecha_fin)) {
        printf("ERROR;Fecha_Invalida\n");
        return -1;
    }
    if (strcmp(fecha_inicio, fecha_fin) > 0) {
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
    char filtro[128] = "";
    Oid paramTypes[4] = {25, 25, 0, 0};
    const char *paramValues[4] = {fecha_inicio, fecha_fin, NULL, NULL};
    int paramLengths[4] = {
        (int)strlen(fecha_inicio), (int)strlen(fecha_fin), 0, 0
    };
    const int paramFormats[4] = {0, 0, 0, 0};
    int nparams = 2;

    if (usarFiltroVendedor) {
        nparams++;
        snprintf(filtro + strlen(filtro), sizeof(filtro) - strlen(filtro),
                 " AND v.id_vendedor = $%d::integer", nparams);
        paramTypes[nparams - 1] = 23;
        paramValues[nparams - 1] = id_vendedor;
        paramLengths[nparams - 1] = (int)strlen(id_vendedor);
    }

    if (usarFiltroCliente) {
        nparams++;
        snprintf(filtro + strlen(filtro), sizeof(filtro) - strlen(filtro),
                 " AND v.dni_cliente = $%d", nparams);
        paramTypes[nparams - 1] = 25;
        paramValues[nparams - 1] = dni_cliente;
        paramLengths[nparams - 1] = (int)strlen(dni_cliente);
    }

    snprintf(sql, sizeof(sql),
        "SELECT TO_CHAR(v.fecha_hora, 'YYYY-MM-DD HH24:MI:SS'), u.nombre, p.nombre, dv.cantidad, dv.subtotal "
        "FROM ventas v "
        "INNER JOIN detalle_ventas dv ON dv.id_venta = v.id "
        "INNER JOIN usuarios u ON u.id = v.id_vendedor "
        "INNER JOIN productos p ON p.codigo = dv.codigo_producto "
        "WHERE v.fecha_hora >= CAST($1 AS date) AND v.fecha_hora < (CAST($2 AS date) + INTERVAL '1 day')%s "
        "ORDER BY v.fecha_hora DESC, v.id DESC, dv.codigo_producto ASC",
        filtro);

    PGresult *res = PQexecParams(conn, sql, nparams, paramTypes,
                                 paramValues, paramLengths, paramFormats, 0);

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
        const char *fecha = PQgetvalue(res, i, 0);
        const char *vendedor = PQgetvalue(res, i, 1);
        const char *producto = PQgetvalue(res, i, 2);
        const char *cantidad = PQgetvalue(res, i, 3);
        const char *subtotal = PQgetvalue(res, i, 4);

        printf("OK;FILA;%s;%s;%s;%d;%.2f\n",
               fecha, vendedor, producto, atoi(cantidad),
               strtod(subtotal, NULL));
        impresos++;
    }

    PQclear(res);
    PQfinish(conn);
    conexion = NULL;
    return impresos;
}

/* --- Motor de alertas (stock <= 10 y productos que vencen en menos de 30 dias) --- */

int reporteAlertas(void) {
    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    PGresult *res = PQexec(conn, "SELECT COUNT(*) FROM productos WHERE stock <= 10");
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

    res = PQexec(conn, "SELECT COUNT(*) FROM productos "
                       "WHERE fecha_vencimiento >= CURRENT_DATE "
                       "AND fecha_vencimiento < CURRENT_DATE + INTERVAL '30 days'");
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

int reporteProductoMasVendido(const char *fecha_inicio, const char *fecha_fin) {
    if (fecha_inicio == NULL || fecha_fin == NULL ||
        !fechaValidaCalendario(fecha_inicio) || !fechaValidaCalendario(fecha_fin)) {
        printf("ERROR;Fecha_Invalida\n");
        return -1;
    }
    if (strcmp(fecha_inicio, fecha_fin) > 0) {
        printf("ERROR;Rango_Fechas_Invalido\n");
        return -1;
    }

    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    char sql[] = "SELECT p.codigo, p.nombre, SUM(dv.cantidad) AS total "
                 "FROM ventas v "
                 "INNER JOIN detalle_ventas dv ON dv.id_venta = v.id "
                 "INNER JOIN productos p ON p.codigo = dv.codigo_producto "
                 "WHERE v.fecha_hora >= CAST($1 AS date) AND v.fecha_hora < (CAST($2 AS date) + INTERVAL '1 day') "
                 "GROUP BY p.codigo, p.nombre "
                 "ORDER BY total DESC, p.codigo ASC "
                 "LIMIT 1";
    const Oid paramTypes[] = { 25, 25 };
    const char * const paramValues[] = { fecha_inicio, fecha_fin };
    const int paramLengths[] = { (int)strlen(fecha_inicio), (int)strlen(fecha_fin) };
    const int paramFormats[] = { 0, 0 };
    int resultFormat = 0;

    PGresult *res = PQexecParams(conn, sql, 2, paramTypes, paramValues, paramLengths, paramFormats, resultFormat);
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

    printf("OK;PRODUCTO_MAS_VENDIDO;%s;%s;%d\n", PQgetvalue(res, 0, 0), PQgetvalue(res, 0, 1), atoi(PQgetvalue(res, 0, 2)));
    PQclear(res);
    PQfinish(conn);
    conexion = NULL;
    return 0;
}

/* --- Metrica: ingresos totales por periodo (SUM con COALESCE) --- */

int reporteIngresosTotales(const char *fecha_inicio, const char *fecha_fin) {
    if (fecha_inicio == NULL || fecha_fin == NULL ||
        !fechaValidaCalendario(fecha_inicio) || !fechaValidaCalendario(fecha_fin)) {
        printf("ERROR;Fecha_Invalida\n");
        return -1;
    }
    if (strcmp(fecha_inicio, fecha_fin) > 0) {
        printf("ERROR;Rango_Fechas_Invalido\n");
        return -1;
    }

    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    char sql[] = "SELECT COALESCE(SUM(dv.subtotal), 0) AS total "
                 "FROM ventas v "
                 "INNER JOIN detalle_ventas dv ON dv.id_venta = v.id "
                 "WHERE v.fecha_hora >= CAST($1 AS date) AND v.fecha_hora < (CAST($2 AS date) + INTERVAL '1 day')";
    const Oid paramTypes[] = { 25, 25 };
    const char * const paramValues[] = { fecha_inicio, fecha_fin };
    const int paramLengths[] = { (int)strlen(fecha_inicio), (int)strlen(fecha_fin) };
    const int paramFormats[] = { 0, 0 };
    int resultFormat = 0;

    PGresult *res = PQexecParams(conn, sql, 2, paramTypes, paramValues, paramLengths, paramFormats, resultFormat);
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

int reporteAlertasStock(void) {
    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT codigo, nombre, stock "
             "FROM productos WHERE stock <= %d "
             "ORDER BY stock ASC, nombre ASC",
             STOCK_MINIMO_DEFAULT);

    PGresult *res = PQexec(conn, sql);

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
            printf("OK;STOCK;%s;%s;%d\n",
                   PQgetvalue(res, i, 0),
                   PQgetvalue(res, i, 1),
                   atoi(PQgetvalue(res, i, 2)));
        }
    }

    PQclear(res);
    PQfinish(conn);
    conexion = NULL;
    return 0;
}

/* --- Listado de productos proximos a vencer desde PostgreSQL --- */

int reporteAlertasVencimientos(void) {
    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    const char *sql =
        "SELECT codigo, nombre, TO_CHAR(fecha_vencimiento, 'YYYY-MM-DD') "
        "FROM productos "
        "WHERE fecha_vencimiento >= CURRENT_DATE "
        "AND fecha_vencimiento < CURRENT_DATE + INTERVAL '30 days' "
        "ORDER BY fecha_vencimiento ASC, nombre ASC";

    PGresult *res = PQexec(conn, sql);

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
