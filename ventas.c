#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include "ventas.h"

// 1. SEGURIDAD: Leer credenciales del entorno, con fallback SOLO para desarrollo local
static const char* get_conninfo() {
    const char* env_conn = getenv("FARMACIA_DB_URL");
    if (env_conn != NULL && strlen(env_conn) > 0) {
        return env_conn;
    }
    return "host=localhost port=5432 dbname=inventario_db user=farmacia_admin password=admin123";
}

void buscar_producto_venta(const char* codigo) {
    if (!codigo || strlen(codigo) == 0) {
        printf("ERROR;Codigo de producto invalido\n");
        return;
    }

    PGconn *conn = PQconnectdb(get_conninfo());
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("ERROR;Fallo conexion a BD: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return;
    }

    const char *paramValues[1] = {codigo};
    PGresult *res = PQexecParams(conn, "SELECT nombre, precio, stock FROM productos WHERE codigo = $1", 1, NULL, paramValues, NULL, NULL, 0);

    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("ERROR;Consulta SQL fallida: %s\n", PQerrorMessage(conn));
    } else if (PQntuples(res) > 0) {
        printf("OK;%s;%s;%s\n", PQgetvalue(res, 0, 0), PQgetvalue(res, 0, 1), PQgetvalue(res, 0, 2));
    } else {
        printf("ERROR;Producto no encontrado\n");
    }

    if (res) PQclear(res);
    PQfinish(conn);
}

void procesar_venta(const char* codigo, int cantidad, int id_vendedor, const char* dni_cliente) {
    PGconn *conn = NULL;
    PGresult *res = NULL;
    PGresult *res_rollback = NULL;

    // 2. VALIDACIÓN ESTRICTA DE ENTRADA
    if (!codigo || strlen(codigo) == 0 || !dni_cliente || strlen(dni_cliente) == 0 || cantidad <= 0 || id_vendedor <= 0) {
        printf("ERROR;Parametros de venta invalidos o cantidad cero\n");
        return;
    }

    conn = PQconnectdb(get_conninfo());
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("ERROR;Fallo conexion a BD: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return;
    }

    // INICIO DE TRANSACCIÓN
    res = PQexec(conn, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR;Fallo al iniciar transaccion: %s\n", PQerrorMessage(conn));
        goto cleanup_error;
    }
    PQclear(res); res = NULL;

    // BLOQUEO DE FILA (FOR UPDATE)
    const char *paramValues[1] = {codigo};
    res = PQexecParams(conn, "SELECT precio, stock FROM productos WHERE codigo = $1 FOR UPDATE", 1, NULL, paramValues, NULL, NULL, 0);
    
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        printf("ERROR;Producto no encontrado o bloqueado\n");
        goto cleanup_rollback;
    }

    // 3. CONVERSIÓN SEGURA (Validando que la BD no devuelva vacíos)
    char *precio_raw = PQgetvalue(res, 0, 0);
    char *stock_raw = PQgetvalue(res, 0, 1);
    if (!precio_raw || !stock_raw || strlen(precio_raw) == 0 || strlen(stock_raw) == 0) {
        printf("ERROR;Datos de BD corruptos (Null/Empty)\n");
        goto cleanup_rollback;
    }
    
    double precio = strtod(precio_raw, NULL);
    int stock_actual = (int)strtol(stock_raw, NULL, 10);
    PQclear(res); res = NULL;

    if (stock_actual < cantidad) {
        printf("ERROR;Stock insuficiente (Actual: %d)\n", stock_actual);
        goto cleanup_rollback;
    }

    // ACTUALIZAR STOCK
    char cant_str[12];
    snprintf(cant_str, sizeof(cant_str), "%d", cantidad);
    const char *updateParams[2] = {cant_str, codigo};
    res = PQexecParams(conn, "UPDATE productos SET stock = stock - $1 WHERE codigo = $2", 2, NULL, updateParams, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR;Fallo al actualizar stock: %s\n", PQerrorMessage(conn));
        goto cleanup_rollback;
    }
    PQclear(res); res = NULL;

    // REGISTRAR VENTA (Cabecera)
    char vend_str[12], total_str[32];
    snprintf(vend_str, sizeof(vend_str), "%d", id_vendedor);
    snprintf(total_str, sizeof(total_str), "%.2f", precio * cantidad);
    
    const char *ventaParams[3] = {vend_str, dni_cliente, total_str};
    res = PQexecParams(conn, "INSERT INTO ventas (id_vendedor, dni_cliente, total) VALUES ($1, $2, $3) RETURNING id_venta", 3, NULL, ventaParams, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("ERROR;Fallo al registrar venta: %s\n", PQerrorMessage(conn));
        goto cleanup_rollback;
    }
    
    // 4. STRING SEGURO (snprintf garantiza el \0 y evita desbordamientos de strncpy)
    char id_venta[32];
    snprintf(id_venta, sizeof(id_venta), "%s", PQgetvalue(res, 0, 0));
    PQclear(res); res = NULL;

    // REGISTRAR DETALLE
    char precio_str[32];
    snprintf(precio_str, sizeof(precio_str), "%.2f", precio);
    const char *detalleParams[5] = {id_venta, codigo, cant_str, precio_str, total_str};
    
    res = PQexecParams(conn, "INSERT INTO detalle_ventas (id_venta, codigo_producto, cantidad, precio_unitario, subtotal) VALUES ($1, $2, $3, $4, $5)", 5, NULL, detalleParams, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR;Fallo al registrar detalle: %s\n", PQerrorMessage(conn));
        goto cleanup_rollback;
    }
    PQclear(res); res = NULL;

    // CONFIRMAR TRANSACCIÓN
    res = PQexec(conn, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR;Fallo al confirmar transaccion\n");
        goto cleanup_rollback;
    }

    // CONTRATO IPC RESPETADO
    printf("OK;Venta procesada con exito;%s\n", id_venta);

cleanup_error:
    if (res) PQclear(res);
    if (conn) PQfinish(conn);
    return;

cleanup_rollback:
    // 5. LIMPIEZA TOTAL: Captura punteros perdidos y libera resultados del rollback
    if (res) PQclear(res);
    res_rollback = PQexec(conn, "ROLLBACK");
    if (res_rollback) PQclear(res_rollback);
    if (conn) PQfinish(conn);
}
