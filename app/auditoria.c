#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <libpq-fe.h>

#include "auditoria.h"

/* Mutex global que serializa el acceso al archivo seguridad.log. */
static pthread_mutex_t mutex_log = PTHREAD_MUTEX_INITIALIZER;

void escribir_log_local(const char *mensaje) {
    if (mensaje == NULL) {
        return;
    }

    /* Fecha local en formato ISO. localtime_r evita el buffer estático
       compartido; la sección crítica completa queda dentro del mutex. */
    time_t ahora = time(NULL);
    struct tm tm_local;
    char fecha[32] = "0000-00-00 00:00:00";
    if (localtime_r(&ahora, &tm_local) != NULL) {
        strftime(fecha, sizeof(fecha), "%Y-%m-%d %H:%M:%S", &tm_local);
    }

    pthread_mutex_lock(&mutex_log);
    FILE *archivo = fopen("seguridad.log", "a");
    if (archivo != NULL) {
        fprintf(archivo, "[%s] %s\n", fecha, mensaje);
        fclose(archivo);
    }
    pthread_mutex_unlock(&mutex_log);
}

int registrar_auditoria(PGconn *conn, const char *usuario, const char *accion) {
    if (conn == NULL || accion == NULL || accion[0] == '\0') {
        return -1;
    }

    const char *usuario_val =
        (usuario != NULL && usuario[0] != '\0') ? usuario : "desconocido";

    const char *sql =
        "INSERT INTO auditoria_acciones (usuario, accion) VALUES ($1, $2)";
    const Oid tipos[] = { 25, 25 }; /* text, text */
    const char *valores[] = { usuario_val, accion };

    PGresult *res = PQexecParams(conn, sql, 2, tipos, valores, NULL, NULL, 0);
    if (res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res != NULL) {
            PQclear(res);
        }
        return -1;
    }

    PQclear(res);
    return 0;
}
