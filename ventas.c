cat << 'EOF' > ventas.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include "ventas.h"

const char *conninfo = "host=localhost port=5432 dbname=inventario_db user=farmacia_admin password=admin123";

void buscar_producto_venta(const char* codigo) {
    PGconn *conn = PQconnectdb(conninfo);
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("ERROR;Fallo la conexion a la BD\n");
        PQfinish(conn);
        return;
    }

    const char *paramValues[1] = {codigo};
    PGresult *res = PQexecParams(conn, "SELECT nombre, precio, stock FROM productos WHERE codigo = $1", 1, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        printf("OK;%s;%s;%s\n", PQgetvalue(res, 0, 0), PQgetvalue(res, 0, 1), PQgetvalue(res, 0, 2));
    } else {
        printf("ERROR;Producto no encontrado\n");
    }
    PQclear(res);
    PQfinish(conn);
}

void procesar_venta(const char* codigo, int cantidad, int id_vendedor, const char* dni_cliente) {
    PGconn *conn = PQconnectdb(conninfo);
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("ERROR;Fallo conexion\n");
        PQfinish(conn);
        return;
    }

    char cant_str[12];
    snprintf(cant_str, sizeof(cant_str), "%d", cantidad);

    // 1. Actualizar el stock
    const char *paramValues[2] = {cant_str, codigo};
    PGresult *res = PQexecParams(conn, "UPDATE productos SET stock = stock - $1 WHERE codigo = $2 AND stock >= $1", 2, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) == PGRES_COMMAND_OK && atoi(PQcmdTuples(res)) > 0) {
        printf("OK;Venta procesada y lista para reportes de Azumi\n");
    } else {
        printf("ERROR;Stock insuficiente o producto inexistente\n");
    }
    PQclear(res);
    PQfinish(conn);
}
EOF
