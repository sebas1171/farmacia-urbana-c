#ifndef USUARIOS_H
#define USUARIOS_H

/* CRUD de usuarios contra PostgreSQL (tabla `usuarios`).
   Todas las funciones leen la cadena de conexion de FARMACIA_DB_CONN y
   escriben el resultado en stdout con el contrato OK;... / ERROR;... */

/* OK;<id>;<nombre>;<username>;<rol> por cada usuario activo.
   OK;SIN_RESULTADOS si no hay usuarios. */
void listarUsuarios(void);

/* Inserta un usuario activo.
   Contrato de main.c: crear_usuario <nombre> <username> <password> <rol>
   Roles aceptados: ADMIN/ADMINISTRADOR y VENDEDOR/TECNICO (se normalizan
   a ADMIN y VENDEDOR).
   OK;Usuario_Creado / ERROR;<motivo>. */
void crearUsuario(const char *nombre, const char *username,
                  const char *password, const char *rol);

/* Actualiza el rol de un usuario activo por id.
   Contrato de main.c: editar_usuario <id> <rol>
   OK;Usuario_Actualizado / ERROR;Usuario_No_Encontrado / ERROR;Rol_Invalido. */
void editarUsuario(int id, const char *rol);

/* Baja logica: activo = FALSE.
   Contrato de main.c: eliminar_usuario <id>
   OK;Usuario_Eliminado / ERROR;Usuario_No_Encontrado. */
void eliminarUsuario(int id);

#endif
