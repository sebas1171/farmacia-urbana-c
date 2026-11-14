#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <libpq-fe.h>

#include "inventario.h"
#include "auditoria.h"

/* Conexión PostgreSQL (libpq). Se abre perezosamente con la cadena de
 * conexión de la variable de entorno FARMACIA_DB_CONN y se libera con
 * cerrarConexion(). */
static PGconn *conexion = NULL;

/* Devuelve la conexión activa o NULL si no está disponible. */
static PGconn *getConexion(void) {
    const char *conn_str = getenv("FARMACIA_DB_CONN");
    if (conn_str == NULL || conn_str[0] == '\0') {
        fprintf(stderr, "ERROR;FARMACIA_DB_CONN_no_definida\n");
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
        printf("ERROR;conexion_bd_fallida\n");
        return NULL;
    }
    return conexion;
}

/* Recorre la cadena y rechaza separadores que rompen el contrato IPC/CSV
   (',' separa cantidad, ';' separa campos y los saltos de línea registros). */
static int textoLimpio(const char *s) {
    if (s == NULL) {
        return 0;
    }
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] == ',' || s[i] == ';' || s[i] == '\n' || s[i] == '\r') {
            return 0;
        }
    }
    return 1;
}

/* Formato estricto YYYY-MM-DD: 10 chars, dígitos y guiones en su sitio. */
static int fechaValida(const char *f) {
    if (f == NULL || strlen(f) != sizeof(((Producto *)0)->fecha_vencimiento) - 1) {
        return 0;
    }
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            if (f[i] != '-') {
                return 0;
            }
        } else if (!isdigit((unsigned char)f[i])) {
            return 0;
        }
    }
    return 1;
}

/* Convierte un número decimal no negativo. Devuelve 1 si es válido. */
static int parsearDecimalNoNegativo(const char *texto, double *destino) {
    if (texto == NULL || texto[0] == '\0' || destino == NULL) {
        return 0;
    }
    errno = 0;
    char *fin = NULL;
    double valor = strtod(texto, &fin);
    if (errno == ERANGE || fin == texto || *fin != '\0' ||
        !isfinite(valor) || valor < 0.0) {
        return 0;
    }
    *destino = valor;
    return 1;
}

/* Valida los campos usados por las funciones en memoria. */
static int camposValidos(const char *nombre, float precio, double stock,
                         const char *lote, const char *fecha_vencimiento) {
    if (nombre == NULL || lote == NULL || fecha_vencimiento == NULL) {
        return 0;
    }
    if (strlen(nombre) == 0 || strlen(lote) == 0 || strlen(fecha_vencimiento) == 0) {
        return 0;
    }
    if (strlen(nombre) >= sizeof(((Producto *)0)->nombre) ||
        strlen(lote)   >= sizeof(((Producto *)0)->lote)) {
        return 0;
    }
    if (precio < 0.0f || !isfinite((double)precio) || stock < 0.0) {
        return 0;
    }
    if (!textoLimpio(nombre) || !textoLimpio(lote) || !textoLimpio(fecha_vencimiento)) {
        return 0;
    }
    if (!fechaValida(fecha_vencimiento)) {
        return 0;
    }
    return 1;
}

/* Copia segura forzando terminador '\0'. */
static void copiarCampo(char *dest, size_t tam, const char *src) {
    strncpy(dest, src, tam - 1);
    dest[tam - 1] = '\0';
}

int registrarProducto(Producto inventario[], int *total, int id,
                      const char *nombre, float precio, double stock,
                      const char *lote, const char *fecha_vencimiento) {
    if (inventario == NULL || total == NULL) {
        return -1;
    }
    if (*total < 0 || *total >= MAX_PRODUCTOS) {
        return -1;
    }
    if (id <= 0) {
        return -1;
    }
    if (!camposValidos(nombre, precio, stock, lote, fecha_vencimiento)) {
        return -1;
    }
    if (buscarPorId(inventario, *total, id) != -1) {
        return -1;
    }

    Producto *p = &inventario[*total];
    p->id = id;
    copiarCampo(p->nombre, sizeof(p->nombre), nombre);
    p->precio = precio;
    p->stock = stock;
    copiarCampo(p->lote, sizeof(p->lote), lote);
    copiarCampo(p->fecha_vencimiento, sizeof(p->fecha_vencimiento), fecha_vencimiento);
    (*total)++;
    return 0;
}

int editarProducto(Producto inventario[], int total, int id,
                   const char *nombre, float precio, double stock,
                   const char *lote, const char *fecha_vencimiento) {
    if (inventario == NULL) {
        return -1;
    }
    if (total < 0 || total > MAX_PRODUCTOS) {
        return -1;
    }
    if (id <= 0) {
        return -1;
    }
    if (!camposValidos(nombre, precio, stock, lote, fecha_vencimiento)) {
        return -1;
    }

    int idx = buscarPorId(inventario, total, id);
    if (idx < 0) {
        return -1;
    }

    Producto *p = &inventario[idx];
    copiarCampo(p->nombre, sizeof(p->nombre), nombre);
    p->precio = precio;
    p->stock = stock;
    copiarCampo(p->lote, sizeof(p->lote), lote);
    copiarCampo(p->fecha_vencimiento, sizeof(p->fecha_vencimiento), fecha_vencimiento);
    return 0;
}

void listarProductos(Producto inventario[], int total) {
    if (inventario == NULL || total <= 0) {
        return;
    }
    for (int i = 0; i < total; i++) {
        printf("%d;%s;%.2f;%.2f;%s;%s\n",
               inventario[i].id,
               inventario[i].nombre,
               inventario[i].precio,
               inventario[i].stock,
               inventario[i].lote,
               inventario[i].fecha_vencimiento);
    }
}

/* --- PostgreSQL (libpq): generación de códigos y búsqueda --- */

int generarCodigoProducto(int id_categoria, char *buffer, size_t tam) {
    if (buffer == NULL || tam == 0 || id_categoria <= 0) {
        return -1;
    }

    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    /* init_db.sql no define la tabla `categorias`. Usamos el id numérico
       como prefijo estable: MED-<id_categoria:03d>-<secuencia:03d>. */
    char prefijo[16];
    snprintf(prefijo, sizeof(prefijo), "MED-%03d", id_categoria);

    char patron[32];
    snprintf(patron, sizeof(patron), "%s-%%", prefijo);

    const char *sql =
        "SELECT COALESCE(MAX(CASE "
        "  WHEN split_part(codigo_producto, '-', 3) ~ '^[0-9]+$' "
        "  THEN split_part(codigo_producto, '-', 3)::integer ELSE 0 END), 0) "
        "FROM producto WHERE codigo_producto LIKE $1";
    const Oid paramTypes[] = { 25 }; /* OID de text */
    const char *const paramValues[] = { patron };
    PGresult *res = PQexecParams(conn, sql, 1, paramTypes, paramValues,
                                 NULL, NULL, 0);

    if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "ERROR;consulta_codigo_fallida: %s\n", PQerrorMessage(conn));
        printf("ERROR;consulta_codigo_fallida\n");
        if (res != NULL) {
            PQclear(res);
        }
        return -1;
    }

    int siguiente = atoi(PQgetvalue(res, 0, 0)) + 1;
    int len = snprintf(buffer, tam, "%s-%03d", prefijo, siguiente);
    PQclear(res);

    if (len < 0 || (size_t)len >= tam) {
        fprintf(stderr, "ERROR;buffer_codigo_corto\n");
        printf("ERROR;buffer_codigo_corto\n");
        return -1;
    }

    return 0;
}

void buscarProductos(const char *texto, int id_sucursal) {
    PGconn *conn = getConexion();
    if (conn == NULL) {
        return;
    }

    if (id_sucursal <= 0) {
        printf("ERROR;Sucursal_Invalida\n");
        return;
    }

    /* Texto vacío (o NULL) no es un error: se usa la cadena vacía como
       comodín para devolver el inventario inicial (primeros 200 productos). */
    const char *termino = (texto == NULL) ? "" : texto;

    char sucursal_txt[16];
    snprintf(sucursal_txt, sizeof(sucursal_txt), "%d", id_sucursal);

    /* El patrón se compone directamente en SQL: ILIKE '%' || $1 || '%'.
       $2 aísla el inventario de la sucursal de la sesión. */
    const Oid paramTypes[] = { 25, 23 }; /* text, integer */
    const int paramLengths[] = {
        (int)strlen(termino), (int)strlen(sucursal_txt)
    };
    const char *paramValues[] = { termino, sucursal_txt };
    const int paramFormats[] = { 0, 0 };
    int resultFormat = 0;

    /* Contrato: OK;Codigo;Nombre;Precio;Stock;Vencimiento;Categoria;Requiere_Receta
       La fecha se obtiene como texto via TO_CHAR con formato YYYY-MM-DD,
       de modo que PostgreSQL valida el tipo DATE antes de devolverla.
       El índice 4 (base 0) es siempre la fecha de vencimiento, el índice
       5 la categoría y el índice 6 si el producto requiere receta (1/0). */
    char sql[] = "SELECT codigo_producto, nombre, precio, stock, "
                 "COALESCE(TO_CHAR(fecha_vencimiento, 'YYYY-MM-DD'), ''), "
                 "COALESCE(categoria, 'Medicamento'), "
                 "COALESCE(requiere_receta, FALSE) "
                 "FROM producto "
                 "WHERE (codigo_producto ILIKE '%' || $1 || '%' "
                 "   OR nombre ILIKE '%' || $1 || '%') "
                 "AND id_sucursal = $2 "
                 "ORDER BY nombre ASC LIMIT 200";
    PGresult *res = PQexecParams(conn, sql, 2, paramTypes, paramValues,
                                 paramLengths, paramFormats, resultFormat);

    if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "ERROR;consulta_productos_fallida: %s\n", PQerrorMessage(conn));
        printf("ERROR;consulta_productos_fallida\n");
        if (res != NULL) {
            PQclear(res);
        }
        return;
    }

    int filas = PQntuples(res);
    if (filas == 0) {
        /* Sin coincidencias no es un fallo: la interfaz limpia la tabla. */
        printf("OK;SIN_RESULTADOS\n");
        PQclear(res);
        return;
    }

    for (int i = 0; i < filas; i++) {
        const char *codigo = PQgetvalue(res, i, 0);
        const char *nombre = PQgetvalue(res, i, 1);
        const char *precio = PQgetvalue(res, i, 2);
        const char *stock = PQgetvalue(res, i, 3);
        const char *fecha = PQgetvalue(res, i, 4);
        const char *categoria = PQgetvalue(res, i, 5);
        const char *requiere_receta = PQgetvalue(res, i, 6);

        /* Contrato fijo de 7 columnas:
           Código; Nombre; Precio; Stock decimal; Vencimiento; Categoría;
           Requiere_Receta (1/0). */
        printf("OK;%s;%s;%.2f;%.2f;%s;%s;%s\n",
               codigo, nombre, strtod(precio, NULL),
               strtod(stock, NULL), fecha, categoria,
               (strcmp(requiere_receta, "t") == 0) ? "1" : "0");
    }

    PQclear(res);
}

/* Búsqueda exacta por código, usada por el IPC `buscar_prod_codigo`.
   Contrato: OK;Codigo;Nombre;Precio;Stock;Vencimiento */
void buscarProductoPorCodigo(const char *codigo, int id_sucursal) {
    if (codigo == NULL || codigo[0] == '\0') {
        printf("ERROR;Codigo_Vacio\n");
        return;
    }
    if (id_sucursal <= 0) {
        printf("ERROR;Sucursal_Invalida\n");
        return;
    }

    PGconn *conn = getConexion();
    if (conn == NULL) {
        return;
    }

    char sucursal_txt[16];
    snprintf(sucursal_txt, sizeof(sucursal_txt), "%d", id_sucursal);

    const char *sql =
        "SELECT codigo_producto, nombre, precio, stock, "
        "COALESCE(TO_CHAR(fecha_vencimiento, 'YYYY-MM-DD'), '') "
        "FROM producto WHERE codigo_producto = $1 "
        "AND id_sucursal = $2 LIMIT 1";
    const Oid paramTypes[] = { 25, 23 };
    const char *const paramValues[] = { codigo, sucursal_txt };
    PGresult *res = PQexecParams(conn, sql, 2, paramTypes, paramValues,
                                 NULL, NULL, 0);

    if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "ERROR;consulta_producto_codigo_fallida: %s\n",
                PQerrorMessage(conn));
        printf("ERROR;consulta_producto_fallida\n");
        if (res != NULL) {
            PQclear(res);
        }
        return;
    }

    if (PQntuples(res) == 0) {
        printf("OK;SIN_RESULTADOS\n");
        PQclear(res);
        return;
    }

    printf("OK;%s;%s;%.2f;%.2f;%s\n",
           PQgetvalue(res, 0, 0),
           PQgetvalue(res, 0, 1),
           strtod(PQgetvalue(res, 0, 2), NULL),
           strtod(PQgetvalue(res, 0, 3), NULL),
           PQgetvalue(res, 0, 4));
    PQclear(res);
}

/* --- CRUD de inventario contra PostgreSQL --- */

/* Genera un código de barras EAN-13 de Perú: prefijo 775 + 9 dígitos
   aleatorios + dígito verificador. El resultado siempre tiene 13 caracteres
   y es un EAN-13 válido (no solo 13 dígitos al azar). */
static void generarCodigoBarrasAUTO(char *buffer, size_t tam) {
    if (buffer == NULL || tam < 14) {
        if (buffer != NULL && tam > 0) {
            buffer[0] = '\0';
        }
        return;
    }

    static int semilla_iniciada = 0;
    if (!semilla_iniciada) {
        /* Cada petición IPC es un proceso nuevo; se combina el reloj con el
           PID para que dos altas simultáneas no generen la misma secuencia. */
        srand((unsigned)time(NULL) ^ ((unsigned)getpid() << 16));
        semilla_iniciada = 1;
    }

    buffer[0] = '7';
    buffer[1] = '7';
    buffer[2] = '5';
    for (int i = 3; i < 12; i++) {
        buffer[i] = (char)('0' + (rand() % 10));
    }

    /* Dígito verificador EAN-13: posiciones impares *1, pares *3. */
    int suma = 0;
    for (int i = 0; i < 12; i++) {
        int digito = buffer[i] - '0';
        suma += (i % 2 == 0) ? digito : digito * 3;
    }
    int verificador = (10 - (suma % 10)) % 10;
    buffer[12] = (char)('0' + verificador);
    buffer[13] = '\0';
}

/* La categoría es opcional; si no se informa se usa el valor por defecto
   del esquema para no perder compatibilidad con clientes antiguos. */
static const char *normalizarCategoria(const char *categoria) {
    return (categoria != NULL && categoria[0] != '\0')
        ? categoria : "Medicamento";
}

/* Valida los datos recibidos desde la interfaz. lote, fecha y categoria son
   opcionales. */
static int validarProductoDB(const char *codigo, const char *nombre,
                             const char *precio_txt, const char *stock_txt,
                             const char *lote, const char *fecha,
                             const char *categoria,
                             double *precio_out, double *stock_out) {
    if (codigo == NULL || nombre == NULL || precio_txt == NULL || stock_txt == NULL) {
        return 0;
    }
    if (codigo[0] == '\0' || strlen(codigo) > 13) {
        return 0;
    }
    if (nombre[0] == '\0' || strlen(nombre) > 100) {
        return 0;
    }
    if (lote != NULL && strlen(lote) > 50) {
        return 0;
    }
    if (fecha != NULL && fecha[0] != '\0' && !fechaValida(fecha)) {
        return 0;
    }
    if (categoria != NULL && strlen(categoria) > 50) {
        return 0;
    }
    if (!textoLimpio(codigo) || !textoLimpio(nombre)) {
        return 0;
    }
    if (lote != NULL && lote[0] != '\0' && !textoLimpio(lote)) {
        return 0;
    }
    if (categoria != NULL && categoria[0] != '\0' && !textoLimpio(categoria)) {
        return 0;
    }
    if (!parsearDecimalNoNegativo(precio_txt, precio_out) ||
        !parsearDecimalNoNegativo(stock_txt, stock_out)) {
        return 0;
    }
    return 1;
}

int crear_prod(const char *codigo, const char *nombre, const char *precio_txt,
               const char *stock_txt, const char *lote, const char *fecha,
               const char *categoria, int id_sucursal) {
    /* Código autogenerado: la interfaz envía "AUTO" cuando el usuario deja
       vacío el campo Código. También se acepta la cadena vacía por si un
       cliente antiguo no aplica esa normalización. Se reemplaza por un
       EAN-13 peruano (775...) antes de insertar. */
    char codigo_auto[14];
    const char *codigo_val = codigo;
    if (codigo == NULL || codigo[0] == '\0' || strcmp(codigo, "AUTO") == 0) {
        generarCodigoBarrasAUTO(codigo_auto, sizeof(codigo_auto));
        codigo_val = codigo_auto;
    }

    const char *categoria_val = normalizarCategoria(categoria);
    double precio = 0.0;
    double stock = 0.0;
    if (!validarProductoDB(codigo_val, nombre, precio_txt, stock_txt,
                           lote, fecha, categoria_val, &precio, &stock)) {
        printf("ERROR;Datos_Producto_Invalidos\n");
        return -1;
    }
    if (id_sucursal <= 0) {
        printf("ERROR;Sucursal_Invalida\n");
        return -1;
    }

    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    char precio_val[32];
    char stock_val[32];
    char sucursal_val[16];
    snprintf(precio_val, sizeof(precio_val), "%.2f", precio);
    snprintf(stock_val, sizeof(stock_val), "%.2f", stock);
    snprintf(sucursal_val, sizeof(sucursal_val), "%d", id_sucursal);

    const char *lote_val = (lote != NULL && lote[0] != '\0') ? lote : NULL;
    const char *fecha_val = (fecha != NULL && fecha[0] != '\0') ? fecha : NULL;

    const char *sql =
        "INSERT INTO producto "
        "(codigo_producto, nombre, precio, stock, lote, fecha_vencimiento, "
        " categoria, id_sucursal) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8)";
    const Oid tipos[] = { 25, 25, 1700, 1700, 25, 1082, 25, 23 };
    const char *valores[] = {
        codigo_val, nombre, precio_val, stock_val, lote_val, fecha_val,
        categoria_val, sucursal_val
    };

    PGresult *res = PQexecParams(conn, sql, 8, tipos, valores, NULL, NULL, 0);
    if (res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
        const char *sqlstate = res != NULL
            ? PQresultErrorField(res, PG_DIAG_SQLSTATE) : NULL;
        if (sqlstate != NULL && strcmp(sqlstate, "23505") == 0) {
            printf("ERROR;Codigo_Ya_Existe\n");
        } else {
            fprintf(stderr, "inventario INSERT: %s\n", PQerrorMessage(conn));
            printf("ERROR;registro_producto_fallido\n");
        }
        if (res != NULL) {
            PQclear(res);
        }
        return -1;
    }

    PQclear(res);
    printf("OK;Producto_Creado\n");
    return 0;
}

int editar_prod(const char *codigo, const char *nombre, const char *precio_txt,
                const char *stock_txt, const char *lote, const char *fecha,
                const char *categoria, int id_sucursal) {
    const char *categoria_val = normalizarCategoria(categoria);
    double precio = 0.0;
    double stock = 0.0;
    if (!validarProductoDB(codigo, nombre, precio_txt, stock_txt,
                           lote, fecha, categoria_val, &precio, &stock)) {
        printf("ERROR;Datos_Producto_Invalidos\n");
        return -1;
    }
    if (id_sucursal <= 0) {
        printf("ERROR;Sucursal_Invalida\n");
        return -1;
    }

    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    char precio_val[32];
    char stock_val[32];
    char sucursal_val[16];
    snprintf(precio_val, sizeof(precio_val), "%.2f", precio);
    snprintf(stock_val, sizeof(stock_val), "%.2f", stock);
    snprintf(sucursal_val, sizeof(sucursal_val), "%d", id_sucursal);

    const char *lote_val = (lote != NULL && lote[0] != '\0') ? lote : NULL;
    const char *fecha_val = (fecha != NULL && fecha[0] != '\0') ? fecha : NULL;

    const char *sql =
        "UPDATE producto SET nombre = $2, precio = $3, stock = $4, "
        "lote = $5, fecha_vencimiento = $6, categoria = $7 "
        "WHERE codigo_producto = $1 AND id_sucursal = $8";
    const Oid tipos[] = { 25, 25, 1700, 1700, 25, 1082, 25, 23 };
    const char *valores[] = {
        codigo, nombre, precio_val, stock_val, lote_val, fecha_val,
        categoria_val, sucursal_val
    };

    PGresult *res = PQexecParams(conn, sql, 8, tipos, valores, NULL, NULL, 0);
    if (res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "inventario UPDATE: %s\n", PQerrorMessage(conn));
        printf("ERROR;actualizacion_producto_fallida\n");
        if (res != NULL) {
            PQclear(res);
        }
        return -1;
    }

    if (strcmp(PQcmdTuples(res), "0") == 0) {
        PQclear(res);
        printf("ERROR;Producto_No_Encontrado\n");
        return -1;
    }

    PQclear(res);

    /* Auditoría: deja constancia de la edición y del usuario activo. */
    char accion[320];
    snprintf(accion, sizeof(accion),
             "Editar producto %s (sucursal %d): nombre=%s, precio=%.2f, "
             "stock=%.2f, lote=%s, vencimiento=%s, categoria=%s",
             codigo, id_sucursal, nombre, precio, stock,
             (lote_val != NULL) ? lote_val : "-",
             (fecha_val != NULL) ? fecha_val : "-", categoria_val);
    registrar_auditoria(conn, getenv("FARMACIA_USER"), accion);

    printf("OK;Producto_Actualizado\n");
    return 0;
}

int eliminar_prod(const char *codigo, int id_sucursal) {
    if (codigo == NULL || codigo[0] == '\0' || strlen(codigo) > 13) {
        printf("ERROR;Codigo_Invalido\n");
        return -1;
    }
    if (id_sucursal <= 0) {
        printf("ERROR;Sucursal_Invalida\n");
        return -1;
    }

    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    char sucursal_val[16];
    snprintf(sucursal_val, sizeof(sucursal_val), "%d", id_sucursal);

    const char *sql =
        "DELETE FROM producto "
        "WHERE codigo_producto = $1 AND id_sucursal = $2";
    const Oid tipos[] = { 25, 23 };
    const char *valores[] = { codigo, sucursal_val };

    PGresult *res = PQexecParams(conn, sql, 2, tipos, valores, NULL, NULL, 0);
    if (res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
        const char *sqlstate = res != NULL
            ? PQresultErrorField(res, PG_DIAG_SQLSTATE) : NULL;
        if (sqlstate != NULL && strcmp(sqlstate, "23503") == 0) {
            printf("ERROR;Producto_Con_Ventas_Asociadas\n");
        } else {
            fprintf(stderr, "inventario DELETE: %s\n", PQerrorMessage(conn));
            printf("ERROR;eliminacion_producto_fallida\n");
        }
        if (res != NULL) {
            PQclear(res);
        }
        return -1;
    }

    if (strcmp(PQcmdTuples(res), "0") == 0) {
        PQclear(res);
        printf("ERROR;Producto_No_Encontrado\n");
        return -1;
    }

    PQclear(res);

    /* Auditoría: deja constancia de la eliminación y del usuario activo. */
    char accion[192];
    snprintf(accion, sizeof(accion),
             "Eliminar producto %s (sucursal %d)", codigo, id_sucursal);
    registrar_auditoria(conn, getenv("FARMACIA_USER"), accion);

    printf("OK;Producto_Eliminado\n");
    return 0;
}

void cerrarConexion(void) {
    if (conexion != NULL) {
        PQfinish(conexion);
        conexion = NULL;
    }
}
