#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>

/**
 * Valida el usuario, contraseña y estado activo en la base de datos PostgreSQL.
 * Imprime el resultado exacto: LOGIN_OK;ROL;NOMBRE o ERROR;Credenciales.
 */
void validar_login(const char *username, const char *password) {
    char conninfo[256];
    snprintf(conninfo, sizeof(conninfo), "dbname=farmacia user=postgres password=tu_password host=localhost port=5432");

    PGconn *conn = PQconnectdb(conninfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        printf("ERROR;Credenciales\n");
        PQfinish(conn);
        return;
    }

    // Validamos credenciales y que el usuario esté activo (activo = TRUE)
    const char *query = "SELECT rol, nombre FROM usuarios WHERE username = $1 AND password = $2 AND activo = TRUE";
    const char *paramValues[2] = { username, password };

    PGresult *res = PQexecParams(conn, query, 2, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        char *rol = PQgetvalue(res, 0, 0);
        char *nombre = PQgetvalue(res, 0, 1);
        printf("LOGIN_OK;%s;%s\n", rol, nombre);
    } else {
        printf("ERROR;Credenciales\n");
    }

    PQclear(res);
    PQfinish(conn);
}