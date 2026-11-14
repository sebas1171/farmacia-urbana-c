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
    if (strcasecmp(rol, "TECNICO") == 0 ||
        strcasecmp(rol, "TÉCNICO") == 0) {
        return "TECNICO";
    }
    if (strcasecmp(rol, "VENDEDOR") == 0) {
        return "VENDEDOR";
    }
    return NULL;
}

/* Lista el catalogo de sucursales (alimenta el combo del CRUD de usuarios).
   Contrato: OK;SUCURSAL;<id>;<nombre>;<direccion> */
void listarSucursales(void) {
    PGconn *conn = conectar();
    if (conn == NULL) {
        return;
    }

    PGresult *res = PQexec(conn,
        "SELECT id, nombre, direccion FROM sucursal ORDER BY id ASC");

    if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "sucursales SELECT: %s\n", PQerrorMessage(conn));
        printf("ERROR;consulta_sucursales_fallida\n");
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
            printf("OK;SUCURSAL;%s;%s;%s\n",
                   PQgetvalue(res, i, 0),
                   PQgetvalue(res, i, 1),
                   PQgetvalue(res, i, 2));
        }
    }

    PQclear(res);
    PQfinish(conn);
}

/* Lista usuarios activos de todas las sucursales. Los de la sucursal de la
   sesión se muestran primero. Contrato:
     OK;<id>;<nombre>;<username>;<rol>;<id_sucursal>;<nombre_sucursal> */
void listarUsuarios(int id_sucursal) {
    if (id_sucursal <= 0) {
        printf("ERROR;Sucursal_Invalida\n");
        return;
    }

    PGconn *conn = conectar();
    if (conn == NULL) {
        return;
    }

    char sucursal_txt[16];
    snprintf(sucursal_txt, sizeof(sucursal_txt), "%d", id_sucursal);

    const char *sql =
        "SELECT u.id, u.nombre, u.username, u.rol, u.id_sucursal, s.nombre "
        "FROM usuarios u "
        "INNER JOIN sucursal s ON s.id = u.id_sucursal "
        "WHERE u.activo = TRUE "
        "ORDER BY (u.id_sucursal = $1) DESC, u.id ASC";
    const Oid paramTypes[] = { 23 };
    const char *const paramValues[] = { sucursal_txt };

    PGresult *res = PQexecParams(conn, sql, 1, paramTypes, paramValues,
                                 NULL, NULL, 0);

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
            printf("OK;%s;%s;%s;%s;%s;%s\n",
                   PQgetvalue(res, i, 0),
                   PQgetvalue(res, i, 1),
                   PQgetvalue(res, i, 2),
                   PQgetvalue(res, i, 3),
                   PQgetvalue(res, i, 4),
                   PQgetvalue(res, i, 5));
        }
    }

    PQclear(res);
    PQfinish(conn);
}

/* Rechaza los caracteres que rompen el contrato IPC (campos ';', líneas). */
static int campoLimpio(const char *s) {
    if (s == NULL) {
        return 0;
    }
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] == ';' || s[i] == '\n' || s[i] == '\r') {
            return 0;
        }
    }
    return 1;
}

void crearUsuario(const char *nombre, const char *username,
                  const char *password, const char *rol, int id_sucursal) {
    if (nombre == NULL || username == NULL || password == NULL || rol == NULL ||
        nombre[0] == '\0' || username[0] == '\0' || password[0] == '\0') {
        printf("ERROR;Campos_Incompletos\n");
        return;
    }
    if (id_sucursal <= 0) {
        printf("ERROR;Sucursal_Invalida\n");
        return;
    }
    if (strlen(nombre) >= 100 || strlen(username) >= 32 ||
        strlen(password) >= 32) {
        printf("ERROR;Campos_Demasiado_Largos\n");
        return;
    }
    if (!campoLimpio(nombre) || !campoLimpio(username)) {
        printf("ERROR;Caracteres_Invalidos\n");
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

    char sucursal_txt[16];
    snprintf(sucursal_txt, sizeof(sucursal_txt), "%d", id_sucursal);

    const char *sql =
        "INSERT INTO usuarios "
        "(username, password, nombre, rol, activo, id_sucursal) "
        "VALUES ($1, $2, $3, $4, TRUE, $5)";
    const Oid paramTypes[] = { 25, 25, 25, 25, 23 };
    const char *const paramValues[] = {
        username, password, nombre, rol_db, sucursal_txt
    };
    PGresult *res = PQexecParams(conn, sql, 5, paramTypes, paramValues,
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

void editarUsuario(int id, const char *rol, int id_sucursal) {
    if (id <= 0) {
        printf("ERROR;ID_Invalido\n");
        return;
    }
    if (id_sucursal <= 0) {
        printf("ERROR;Sucursal_Invalida\n");
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
    char sucursal_txt[16];
    snprintf(id_txt, sizeof(id_txt), "%d", id);
    snprintf(sucursal_txt, sizeof(sucursal_txt), "%d", id_sucursal);

    const char *sql =
        "UPDATE usuarios SET rol = $2, id_sucursal = $3 "
        "WHERE id = $1 AND activo = TRUE";
    const Oid paramTypes[] = { 23, 25, 23 };
    const char *const paramValues[] = { id_txt, rol_db, sucursal_txt };
    PGresult *res = PQexecParams(conn, sql, 3, paramTypes, paramValues,
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
