#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <libpq-fe.h>

#include "inventario.h"

/* Conexión PostgreSQL (libpq). Se abre perezosamente con la cadena de
 * conexión de la variable de entorno FARMACIA_DB_CONN y se libera con
 * cerrarConexion(). */
static PGconn *conexion = NULL;

/* Devuelve la conexión activa o NULL si no está disponible. */
static PGconn *getConexion() {
    const char *conn_str = getenv("FARMACIA_DB_CONN");
    if (conn_str == NULL) {
        fprintf(stderr, "ERROR;FARMACIA_DB_CONN_no_definida\n");
        return NULL;
    }
    if (conexion == NULL) {
        conexion = PQconnectdb(conn_str);
    }
    if (PQstatus(conexion) != CONNECTION_OK) {
        fprintf(stderr, "ERROR;conexion_farmacia_fallida\n");
        return NULL;
    }
    return conexion;
}

/* Recorre la cadena y rechaza comas y saltos de línea (rompen el CSV). */
static int textoLimpio(const char *s) {
    if (s == NULL) {
        return 0;
    }
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] == ',' || s[i] == '\n' || s[i] == '\r') {
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

/* Valida los campos comunes. Devuelve 1 si todo es correcto. */
static int camposValidos(const char *nombre, float precio, int stock,
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
    if (precio < 0.0f || stock < 0) {
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
                      const char *nombre, float precio, int stock,
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
                   const char *nombre, float precio, int stock,
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
        printf("%d;%s;%.2f;%d;%s;%s\n",
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
    PGconn *conn = getConexion();
    if (conn == NULL) {
        return -1;
    }

    /* Obtiene las primeras tres letras del nombre de la categoría en mayúsculas. */
    char idStr[32];
    snprintf(idStr, sizeof(idStr), "%d", id_categoria);
    const Oid paramTypes[] = { 21 }; /* OID de int4 */
    const char * const paramValues[] = { idStr };
    const int paramLengths[] = { (int)strlen(idStr) };
    const int paramFormats[] = { 0 };
    int resultFormat = 0;

    char sql[] = "SELECT UPPER(LEFT(nombre, 3)) FROM categorias WHERE id = $1::integer";
    PGresult *res = PQexecParams(conn, sql, 1, paramTypes, paramValues, paramLengths, paramFormats, resultFormat);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "ERROR;consulta_categoria_fallida: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }
    if (PQntuples(res) < 1) {
        printf("ERROR;Categoria_No_Encontrada\n");
        PQclear(res);
        return -1;
    }

    char prefix[8];
    strncpy(prefix, PQgetvalue(res, 0, 0), sizeof(prefix) - 1);
    prefix[sizeof(prefix) - 1] = '\0';
    PQclear(res);

    /* Obtiene el mayor número registrado para el prefijo de la categoría
       (ej. MED-PAR-001, MED-PAR-003) y genera el siguiente con tres dígitos. */
    char sqlCount[] = "SELECT COALESCE(MAX(CAST(split_part(codigo, '-', 3) AS INTEGER)), 0) FROM productos WHERE codigo LIKE 'MED-' || $1 || '-%'";
    const Oid paramTypesText[] = { 25 }; /* OID de text */
    const char *const paramValuesText[] = { prefix };
    const int paramLengthsText[] = { (int)strlen(prefix) };
    PGresult *resCount = PQexecParams(conn, sqlCount, 1, paramTypesText, paramValuesText, paramLengthsText, paramFormats, resultFormat);

    if (PQresultStatus(resCount) != PGRES_TUPLES_OK) {
        fprintf(stderr, "ERROR;consulta_codigo_fallida\n");
        PQclear(resCount);
        return -1;
    }

    int total = atoi(PQgetvalue(resCount, 0, 0));
    int siguiente = total + 1;
    int len = snprintf(buffer, tam, "MED-%s-%03d", prefix, siguiente);
    if (len < 0 || (size_t)len >= tam) {
        fprintf(stderr, "ERROR;buffer_codigo_corto\n");
        PQclear(resCount);
        return -1;
    }
    PQclear(resCount);

    return 0;
}

void buscarProductos(const char *texto) {
    PGconn *conn = getConexion();
    if (conn == NULL) {
        return;
    }

    if (texto == NULL || strlen(texto) == 0) {
        printf("ERROR;Producto_No_Encontrado\n");
        return;
    }

    /* Patrón con comodines para la búsqueda por ILIKE, pasado como parámetro. */
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%%%s%%", texto);
    const Oid paramTypes[] = { 25 }; /* OID de text */
    const int paramLengths[] = { (int)strlen(pattern) };
    const char *paramValues[] = { pattern };
    const int paramFormats[] = { 0 };
    int resultFormat = 0;

    /* La fecha se obtiene como texto via TO_CHAR con formato YYYY-MM-DD,
       de modo que PostgreSQL valida el tipo DATE antes de devolverla. */
    char sql[] = "SELECT codigo, nombre, precio, stock, TO_CHAR(fecha_vencimiento, 'YYYY-MM-DD') "
                 "FROM productos WHERE codigo ILIKE $1 OR nombre ILIKE $1";
    PGresult *res = PQexecParams(conn, sql, 1, paramTypes, paramValues, paramLengths, paramFormats, resultFormat);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "ERROR;consulta_productos_fallida\n");
        PQclear(res);
        return;
    }

    int filas = PQntuples(res);
    if (filas == 0) {
        printf("ERROR;Producto_No_Encontrado\n");
        PQclear(res);
        return;
    }

    for (int i = 0; i < filas; i++) {
        const char *codigo = PQgetvalue(res, i, 0);
        const char *nombre = PQgetvalue(res, i, 1);
        const char *precio = PQgetvalue(res, i, 2);
        const char *stock = PQgetvalue(res, i, 3);
        const char *fecha = PQgetvalue(res, i, 4);

        printf("OK;%s;%s;%.2f;%d;%s\n", codigo, nombre, strtod(precio, NULL), atoi(stock), fecha);
    }

    PQclear(res);
}

void cerrarConexion(void) {
    if (conexion != NULL) {
        PQfinish(conexion);
        conexion = NULL;
    }
}
