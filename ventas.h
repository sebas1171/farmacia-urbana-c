#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include "ventas.h"

// Cadena de conexión centralizada
const char *conninfo = "host=localhost port=5432 dbname=inventario_db user=farmacia_admin password=admin123";

void buscar_producto_venta(const char* codigo) {
    PGconn *conn = PQconnectdb(conninfo);
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("ERROR;Fallo conexion a la BD\n");
        PQfinish(conn);
        return;
    }

    const char *paramValues[1] = {codigo};
    PGresult *res = PQexecParams(conn, "SELECT nombre, precio, stock FROM productos WHERE codigo = $1", 1, NULL, paramValues, NULL, NULL, 0);

    // Validación contra punteros NULL y estado de la consulta
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("ERROR;Fallo en la consulta SQL al buscar producto\n");
    } else if (PQntuples(res) > 0) {
        printf("OK;%s;%s;%s\n", PQgetvalue(res, 0, 0), PQgetvalue(res, 0, 1), PQgetvalue(res, 0, 2));
    } else {
        printf("ERROR;Producto no encontrado\n");
    }

    if (res) PQclear(res);
    PQfinish(conn);
}

void procesar_venta(const char* codigo, int cantidad, int id_vendedor, const char* dni_cliente) {
    // 1. Validación estricta de cantidad
    if (cantidad <= 0) {
        printf("ERROR;La cantidad debe ser mayor a 0\n");
        return;
    }

    PGconn *conn = PQconnectdb(conninfo);
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("ERROR;Fallo conexion a la BD\n");
        PQfinish(conn);
        return;
    }

    // 2. Iniciar Transacción Segura
    PGresult *res = PQexec(conn, "BEGIN");
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR;No se pudo iniciar transaccion\n");
        if (res) PQclear(res);
        PQfinish(conn);
        return;
    }
    PQclear(res);

    // 3. Consultar precio y bloquear fila (FOR UPDATE) para evitar condiciones de carrera
    const char *paramValues[1] = {codigo};
    res = PQexecParams(conn, "SELECT precio, stock FROM productos WHERE codigo = $1 FOR UPDATE", 1, NULL, paramValues, NULL, NULL, 0);
    
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        printf("ERROR;Producto no encontrado o error de lectura\n");
        PQexec(conn, "ROLLBACK");
        if (res) PQclear(res);
        PQfinish(conn);
        return;
    }

    double precio = atof(PQgetvalue(res, 0, 0));
    int stock_actual = atoi(PQgetvalue(res, 0, 1));
    PQclear(res);

    if (stock_actual < cantidad) {
        printf("ERROR;Stock insuficiente\n");
        PQexec(conn, "ROLLBACK");
        PQfinish(conn);
        return;
    }

    // 4. Descontar el Stock
    char cant_str[12];
    snprintf(cant_str, sizeof(cant_str), "%d", cantidad);
    const char *updateParams[2] = {cant_str, codigo};
    res = PQexecParams(conn, "UPDATE productos SET stock = stock - $1 WHERE codigo = $2", 2, NULL, updateParams, NULL, NULL, 0);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR;Fallo al actualizar stock\n");
        PQexec(conn, "ROLLBACK");
        if (res) PQclear(res);
        PQfinish(conn);
        return;
    }
    PQclear(res);

    // 5. Registrar la Venta (Cabecera) para Azumi
    char vend_str[12], total_str[32];
    snprintf(vend_str, sizeof(vend_str), "%d", id_vendedor);
    snprintf(total_str, sizeof(total_str), "%.2f", precio * cantidad);
    
    const char *ventaParams[3] = {vend_str, dni_cliente, total_str};
    // RETURNING id_venta nos permite obtener el ID generado inmediatamente
    res = PQexecParams(conn, "INSERT INTO ventas (id_vendedor, dni_cliente, total) VALUES ($1, $2, $3) RETURNING id_venta", 3, NULL, ventaParams, NULL, NULL, 0);
    
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("ERROR;Fallo al registrar cabecera de venta\n");
        PQexec(conn, "ROLLBACK");
        if (res) PQclear(res);
        PQfinish(conn);
        return;
    }
    
    char id_venta[32];
    strncpy(id_venta, PQgetvalue(res, 0, 0), sizeof(id_venta));
    PQclear(res);

    // 6. Registrar Detalle de Venta
    char precio_str[32];
    snprintf(precio_str, sizeof(precio_str), "%.2f", precio);
    const char *detalleParams[5] = {id_venta, codigo, cant_str, precio_str, total_str};
    
    res = PQexecParams(conn, "INSERT INTO detalle_ventas (id_venta, codigo_producto, cantidad, precio_unitario, subtotal) VALUES ($1, $2, $3, $4, $5)", 5, NULL, detalleParams, NULL, NULL, 0);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR;Fallo al registrar detalle de venta\n");
        PQexec(conn, "ROLLBACK");
        if (res) PQclear(res);
        PQfinish(conn);
        return;
    }
    PQclear(res);

    // 7. Confirmar Transacción
    res = PQexec(conn, "COMMIT");
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR;Fallo al confirmar transaccion en BD\n");
        if (res) PQclear(res);
        PQfinish(conn);
        return;
    }
    PQclear(res);

    // 8. Contrato de Salida IPC intacto para el parser de Python
    printf("OK;Venta procesada;%s\n", id_venta);
    
    PQfinish(conn);
}
