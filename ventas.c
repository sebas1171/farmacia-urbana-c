#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include "ventas.h"

// NOTA DE SEGURIDAD: Para producción, esto debería venir de variables de entorno (getenv).
// Se mantiene hardcodeado temporalmente por compatibilidad estricta con el Docker local.
const char *conninfo = "host=localhost port=5432 dbname=inventario_db user=farmacia_admin password=admin123";

void buscar_producto_venta(const char* codigo) {
    PGconn *conn = PQconnectdb(conninfo);
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
    if (cantidad <= 0) {
        printf("ERROR;La cantidad debe ser mayor a 0\n");
        return;
    }

    PGconn *conn = PQconnectdb(conninfo);
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("ERROR;Fallo conexion a BD: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return;
    }

    // 1. Iniciar Transacción ACID
    PGresult *res = PQexec(conn, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR;Fallo al iniciar transaccion: %s\n", PQerrorMessage(conn));
        PQclear(res); PQfinish(conn); return;
    }
    PQclear(res);

    // 2. Bloquear la fila del producto (Previene Oversell / Race Conditions)
    const char *paramValues[1] = {codigo};
    res = PQexecParams(conn, "SELECT precio, stock FROM productos WHERE codigo = $1 FOR UPDATE", 1, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        printf("ERROR;Producto no encontrado o bloqueado\n");
        PQexec(conn, "ROLLBACK");
        if (res) PQclear(res);
        PQfinish(conn);
        return;
    }

    double precio = atof(PQgetvalue(res, 0, 0));
    int stock_actual = atoi(PQgetvalue(res, 0, 1));
    PQclear(res);

    if (stock_actual < cantidad) {
        printf("ERROR;Stock insuficiente (Actual: %d)\n", stock_actual);
        PQexec(conn, "ROLLBACK");
        PQfinish(conn);
        return;
    }

    // 3. Descontar Stock
    char cant_str[12];
    snprintf(cant_str, sizeof(cant_str), "%d", cantidad);
    const char *updateParams[2] = {cant_str, codigo};
    res = PQexecParams(conn, "UPDATE productos SET stock = stock - $1 WHERE codigo = $2", 2, NULL, updateParams, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR;Fallo al actualizar stock: %s\n", PQerrorMessage(conn));
        PQexec(conn, "ROLLBACK");
        PQclear(res); PQfinish(conn); return;
    }
    PQclear(res);

    // 4. Registrar Venta (Para historial de Azumi)
    char vend_str[12], total_str[32];
    snprintf(vend_str, sizeof(vend_str), "%d", id_vendedor);
    snprintf(total_str, sizeof(total_str), "%.2f", precio * cantidad);
    
    const char *ventaParams[3] = {vend_str, dni_cliente, total_str};
    res = PQexecParams(conn, "INSERT INTO ventas (id_vendedor, dni_cliente, total) VALUES ($1, $2, $3) RETURNING id_venta", 3, NULL, ventaParams, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("ERROR;Fallo al registrar venta: %s\n", PQerrorMessage(conn));
        PQexec(conn, "ROLLBACK");
        PQclear(res); PQfinish(conn); return;
    }
    
    char id_venta[32];
    strncpy(id_venta, PQgetvalue(res, 0, 0), sizeof(id_venta));
    PQclear(res);

    // 5. Registrar Detalle de Venta
    char precio_str[32];
    snprintf(precio_str, sizeof(precio_str), "%.2f", precio);
    const char *detalleParams[5] = {id_venta, codigo, cant_str, precio_str, total_str};
    
    res = PQexecParams(conn, "INSERT INTO detalle_ventas (id_venta, codigo_producto, cantidad, precio_unitario, subtotal) VALUES ($1, $2, $3, $4, $5)", 5, NULL, detalleParams, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR;Fallo al registrar detalle: %s\n", PQerrorMessage(conn));
        PQexec(conn, "ROLLBACK");
        PQclear(res); PQfinish(conn); return;
    }
    PQclear(res);

    // 6. Confirmar Transacción
    res = PQexec(conn, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR;Fallo al confirmar transaccion\n");
        PQclear(res); PQfinish(conn); return;
    }
    PQclear(res);

    printf("OK;Venta procesada con exito;%s\n", id_venta);
    PQfinish(conn);
}
