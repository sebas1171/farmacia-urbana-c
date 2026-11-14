#ifndef USUARIOS_H
#define USUARIOS_H

/* CRUD de usuarios contra PostgreSQL (tabla `usuarios`).
   Todas las funciones leen la cadena de conexion de FARMACIA_DB_CONN y
   escriben el resultado en stdout con el contrato OK;... / ERROR;... */

/* Catalogo de sucursales.
   OK;SUCURSAL;<id>;<nombre>;<direccion> por cada sucursal.
   OK;SIN_RESULTADOS si no hay sucursales. */
void listarSucursales(void);

/* OK;<id>;<nombre>;<username>;<rol>;<id_sucursal>;<nombre_sucursal>
   por cada usuario activo (primero los de la sucursal de la sesión).
   OK;SIN_RESULTADOS si no hay usuarios. */
void listarUsuarios(int id_sucursal);

/* Inserta un usuario activo en la sucursal indicada.
   Contrato de main.c: crear_usuario <nombre> <username> <password> <rol>
                       <id_sucursal>
   Roles aceptados: ADMIN/ADMINISTRADOR y VENDEDOR/TECNICO (se normalizan
   a ADMIN y VENDEDOR).
   OK;Usuario_Creado / ERROR;<motivo>. */
void crearUsuario(const char *nombre, const char *username,
                  const char *password, const char *rol, int id_sucursal);

/* Actualiza el rol y la sucursal de un usuario activo por id.
   Contrato de main.c: editar_usuario <id> <rol> <id_sucursal>
   OK;Usuario_Actualizado / ERROR;Usuario_No_Encontrado / ERROR;Rol_Invalido. */
void editarUsuario(int id, const char *rol, int id_sucursal);

/* Baja logica: activo = FALSE.
   Contrato de main.c: eliminar_usuario <id>
   OK;Usuario_Eliminado / ERROR;Usuario_No_Encontrado. */
void eliminarUsuario(int id);

#endif
