#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>

#include "login_2.h"

/* Valida credenciales contra PostgreSQL usando FARMACIA_DB_CONN. */
void validar_login(const char *username, const char *password) {
    if (username == NULL || password == NULL ||
        username[0] == '\0' || password[0] == '\0') {
        printf("ERROR;Credenciales\n");
        return;
    }

    const char *conninfo = getenv("FARMACIA_DB_CONN");
    if (conninfo == NULL || conninfo[0] == '\0') {
        printf("ERROR;FARMACIA_DB_CONN_no_definida\n");
        return;
    }

    PGconn *conn = PQconnectdb(conninfo);
    if (conn == NULL || PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "login: %s\n",
                conn != NULL ? PQerrorMessage(conn) : "sin conexion");
        printf("ERROR;conexion_bd_fallida\n");
        if (conn != NULL) {
            PQfinish(conn);
        }
        return;
    }

    /* El login devuelve tambien la sucursal del usuario para que la interfaz
       fije la sesion y todas las peticiones posteriores queden aisladas. */
    const char *sql =
        "SELECT u.rol, u.nombre, u.id_sucursal, s.nombre "
        "FROM usuarios u "
        "INNER JOIN sucursal s ON s.id = u.id_sucursal "
        "WHERE u.username = $1 AND u.password = $2 AND u.activo = TRUE "
        "LIMIT 1";
    const Oid paramTypes[] = { 25, 25 };
    const char *const paramValues[] = { username, password };
    PGresult *res = PQexecParams(conn, sql, 2, paramTypes, paramValues,
                                 NULL, NULL, 0);

    if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "login: %s\n",
                res != NULL ? PQerrorMessage(conn) : "sin resultado");
        printf("ERROR;conexion_bd_fallida\n");
        if (res != NULL) {
            PQclear(res);
        }
        PQfinish(conn);
        return;
    }

    if (PQntuples(res) > 0) {
        /* Contrato: LOGIN_OK;<rol>;<nombre>;<id_sucursal>;<nombre_sucursal> */
        printf("LOGIN_OK;%s;%s;%s;%s\n",
               PQgetvalue(res, 0, 0),
               PQgetvalue(res, 0, 1),
               PQgetvalue(res, 0, 2),
               PQgetvalue(res, 0, 3));
    } else {
        printf("ERROR;Credenciales\n");
    }

    PQclear(res);
    PQfinish(conn);
}
