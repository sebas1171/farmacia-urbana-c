#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include "ventas.h"

// Cadena de conexión a la BD de Docker
const char *conninfo = "host=localhost port=5432 dbname=inventario_db user=farmacia_admin password=admin123";

void buscar_producto_venta(const char* codigo) {
    PGconn *conn = PQconnectdb(conninfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        printf("ERROR;Fallo la conexion a la BD: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return;
    }

    const char *paramValues[1] = {codigo};

    PGresult *res = PQexecParams(conn,
                                 "SELECT nombre, precio, stock FROM productos WHERE codigo = $1",
                                 1, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("ERROR;Error al ejecutar la consulta SQL\n");
    } else if (PQntuples(res) > 0) {
        char *nombre = PQgetvalue(res, 0, 0);
        char *precio = PQgetvalue(res, 0, 1);
        char *stock = PQgetvalue(res, 0, 2);
        
        // IMPRESIÓN ESTRICTA PARA EL IPC DE PYTHON
        printf("OK;%s;%s;%s\n", nombre, precio, stock);
    } else {
        printf("ERROR;Producto no encontrado\n");
    }

    PQclear(res);
    PQfinish(conn);
}

void procesar_venta(const char* codigo, int cantidad) {
    PGconn *conn = PQconnectdb(conninfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        printf("ERROR;Fallo la conexion a la BD\n");
        PQfinish(conn);
        return;
    }

    // Convertir la cantidad a string para libpq
    char cantidad_str[12];
    snprintf(cantidad_str, sizeof(cantidad_str), "%d", cantidad);

    const char *paramValues[2] = {cantidad_str, codigo};

    // Actualizar el stock
    PGresult *res = PQexecParams(conn,
                                 "UPDATE productos SET stock = stock - $1 WHERE codigo = $2 AND stock >= $1",
                                 2, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR;Fallo al procesar la venta o stock insuficiente\n");
    } else {
        // Verificar si realmente se actualizó alguna fila
        if (atoi(PQcmdTuples(res)) > 0) {
            printf("OK;Venta procesada y stock actualizado correctamente\n");
        } else {
            printf("ERROR;Stock insuficiente o producto inexistente\n");
        }
    }

    PQclear(res);
    PQfinish(conn);
}
