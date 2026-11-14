#ifndef AUDITORIA_H
#define AUDITORIA_H

#include <libpq-fe.h>

/* ==========================================================================
   Módulo de seguridad y auditoría.

   - escribir_log_local(): bitácora local en seguridad.log, serializada con
     un mutex global (pthread) para que varios hilos no corrompan el archivo.
   - registrar_auditoria(): inserta la acción en la tabla PostgreSQL
     auditoria_acciones reutilizando la conexión de la operación.
   ========================================================================== */

/* Escribe `mensaje` en seguridad.log (modo append) precedido por la fecha y
   hora local. La apertura, escritura y cierre del archivo están protegidas
   por un pthread_mutex global. Es seguro llamarla con mensaje == NULL. */
void escribir_log_local(const char *mensaje);

/* Registra una fila en auditoria_acciones usando `conn`.
   `usuario` es el usuario activo (NULL o "" -> "desconocido") y `accion` el
   detalle de la operación. Devuelve 0 si se registró y -1 si falló; nunca
   aborta la operación principal (la auditoría es best-effort). */
int registrar_auditoria(PGconn *conn, const char *usuario, const char *accion);

#endif /* AUDITORIA_H */
