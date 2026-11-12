#ifndef VENTAS_H
#define VENTAS_H

/* Registra una venta completa en PostgreSQL dentro de una transaccion.

   Parametros:
     dni      DNI de 8 digitos o "" / NULL para venta sin cliente.
     carrito  "CODIGO,cantidad|CODIGO,cantidad|...".
     username Usuario que realiza la venta (se resuelve a usuarios.id) o
              NULL para registrar id_vendedor NULL.

   Contrato de salida (stdout):
     OK;VENTA;<id_venta>;<total>
     ERROR;<motivo>  (Carrito_Vacio, Carrito_Invalido, DNI_Invalido,
                      Stock_Insuficiente:<codigo>, Producto_No_Encontrado:<codigo>,
                      Vendedor_No_Encontrado, conexion_bd_fallida, ...)

   Devuelve 0 si la venta se confirmo (COMMIT) o -1 si se revirtio. */
int procesarVenta(const char *dni, const char *carrito, const char *username);

#endif
