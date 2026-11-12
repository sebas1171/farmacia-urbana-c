#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <libpq-fe.h>

#include "usuarios.h"

static PGconn *conectar(void) {
    const char *conninfo = getenv("FARMACIA_DB_CONN");
    if (conninfo == NULL || conninfo[0] == '\0') {
        printf("ERROR;FARMACIA_DB_CONN_no_definida\n");
        return NULL;
    }

    PGconn *conn = PQconnectdb(conninfo);
    if (conn == NULL || PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "usuarios: %s\n",
                conn != NULL ? PQerrorMessage(conn) : "sin conexion");
        printf("ERROR;conexion_bd_fallida\n");
        if (conn != NULL) {
            PQfinish(conn);
        }
        return NULL;
    }
    return conn;
}

/* Normaliza el vocabulario de roles de la interfaz al de la base de datos. */
static const char *normalizarRol(const char *rol) {
    if (rol == NULL) {
        return NULL;
    }
    if (strcasecmp(rol, "ADMIN") == 0 ||
        strcasecmp(rol, "ADMINISTRADOR") == 0) {
        return "ADMIN";
    }
    if (strcasecmp(rol, "VENDEDOR") == 0 ||
        strcasecmp(rol, "TECNICO") == 0) {
        return "VENDEDOR";
    }
    return NULL;
}

void listarUsuarios(void) {
    PGconn *conn = conectar();
    if (conn == NULL) {
        return;
    }

    PGresult *res = PQexec(conn,
        "SELECT id, nombre, username, rol FROM usuarios "
        "WHERE activo = TRUE ORDER BY id ASC");

    if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "usuarios SELECT: %s\n", PQerrorMessage(conn));
        printf("ERROR;consulta_usuarios_fallida\n");
        if (res != NULL) {
            PQclear(res);
        }
        PQfinish(conn);
        return;
    }

    int filas = PQntuples(res);
    if (filas == 0) {
        printf("OK;SIN_RESULTADOS\n");
    } else {
        for (int i = 0; i < filas; i++) {
            printf("OK;%s;%s;%s;%s\n",
                   PQgetvalue(res, i, 0),
                   PQgetvalue(res, i, 1),
                   PQgetvalue(res, i, 2),
                   PQgetvalue(res, i, 3));
        }
    }

    PQclear(res);
    PQfinish(conn);
}

void crearUsuario(const char *nombre, const char *username,
                  const char *password, const char *rol) {
    if (nombre == NULL || username == NULL || password == NULL || rol == NULL ||
        nombre[0] == '\0' || username[0] == '\0' || password[0] == '\0') {
        printf("ERROR;Campos_Incompletos\n");
        return;
    }
    if (strlen(nombre) >= 100 || strlen(username) >= 32 ||
        strlen(password) >= 32) {
        printf("ERROR;Campos_Demasiado_Largos\n");
        return;
    }

    const char *rol_db = normalizarRol(rol);
    if (rol_db == NULL) {
        printf("ERROR;Rol_Invalido\n");
        return;
    }

    PGconn *conn = conectar();
    if (conn == NULL) {
        return;
    }

    const char *sql =
        "INSERT INTO usuarios (username, password, nombre, rol, activo) "
        "VALUES ($1, $2, $3, $4, TRUE)";
    const Oid paramTypes[] = { 25, 25, 25, 25 };
    const char *const paramValues[] = { username, password, nombre, rol_db };
    PGresult *res = PQexecParams(conn, sql, 4, paramTypes, paramValues,
                                 NULL, NULL, 0);

    if (res == NULL) {
        printf("ERROR;registro_usuario_fallido\n");
        PQfinish(conn);
        return;
    }

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        const char *sqlstate = PQresultErrorField(res, PG_DIAG_SQLSTATE);
        if (sqlstate != NULL && strcmp(sqlstate, "23505") == 0) {
            printf("ERROR;Username_Ya_Existe\n");
        } else {
            fprintf(stderr, "usuarios INSERT: %s\n", PQerrorMessage(conn));
            printf("ERROR;registro_usuario_fallido\n");
        }
    } else {
        printf("OK;Usuario_Creado\n");
    }

    PQclear(res);
    PQfinish(conn);
}

void editarUsuario(int id, const char *rol) {
    if (id <= 0) {
        printf("ERROR;ID_Invalido\n");
        return;
    }

    const char *rol_db = normalizarRol(rol);
    if (rol_db == NULL) {
        printf("ERROR;Rol_Invalido\n");
        return;
    }

    PGconn *conn = conectar();
    if (conn == NULL) {
        return;
    }

    char id_txt[16];
    snprintf(id_txt, sizeof(id_txt), "%d", id);

    const char *sql =
        "UPDATE usuarios SET rol = $2 WHERE id = $1 AND activo = TRUE";
    const Oid paramTypes[] = { 23, 25 };
    const char *const paramValues[] = { id_txt, rol_db };
    PGresult *res = PQexecParams(conn, sql, 2, paramTypes, paramValues,
                                 NULL, NULL, 0);

    if (res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "usuarios UPDATE: %s\n", PQerrorMessage(conn));
        printf("ERROR;actualizacion_usuario_fallida\n");
        if (res != NULL) {
            PQclear(res);
        }
        PQfinish(conn);
        return;
    }

    if (strcmp(PQcmdTuples(res), "0") == 0) {
        printf("ERROR;Usuario_No_Encontrado\n");
    } else {
        printf("OK;Usuario_Actualizado\n");
    }

    PQclear(res);
    PQfinish(conn);
}

void eliminarUsuario(int id) {
    if (id <= 0) {
        printf("ERROR;ID_Invalido\n");
        return;
    }

    PGconn *conn = conectar();
    if (conn == NULL) {
        return;
    }

    char id_txt[16];
    snprintf(id_txt, sizeof(id_txt), "%d", id);

    const char *sql =
        "UPDATE usuarios SET activo = FALSE WHERE id = $1 AND activo = TRUE";
    const Oid paramTypes[] = { 23 };
    const char *const paramValues[] = { id_txt };
    PGresult *res = PQexecParams(conn, sql, 1, paramTypes, paramValues,
                                 NULL, NULL, 0);

    if (res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "usuarios DELETE: %s\n", PQerrorMessage(conn));
        printf("ERROR;eliminacion_usuario_fallida\n");
        if (res != NULL) {
            PQclear(res);
        }
        PQfinish(conn);
        return;
    }

    if (strcmp(PQcmdTuples(res), "0") == 0) {
        printf("ERROR;Usuario_No_Encontrado\n");
    } else {
        printf("OK;Usuario_Eliminado\n");
    }

    PQclear(res);
    PQfinish(conn);
}
