#ifndef LOGIN_2_H
#define LOGIN_2_H

/* Valida usuario/contraseña contra la tabla `usuarios` de PostgreSQL.
   Lee la cadena de conexión de la variable de entorno FARMACIA_DB_CONN.

   Contrato de salida (stdout):
     LOGIN_OK;<rol>;<nombre>;<id_sucursal>;<nombre_sucursal>
     ERROR;Credenciales
     ERROR;FARMACIA_DB_CONN_no_definida
     ERROR;conexion_bd_fallida */
void validar_login(const char *username, const char *password);

#endif
