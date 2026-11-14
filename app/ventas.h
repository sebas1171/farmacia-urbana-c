#ifndef VENTAS_H
#define VENTAS_H

/* Registra una venta completa en PostgreSQL dentro de una transaccion.

   Parametros:
     dni      DNI de 8 digitos o "" / NULL para venta sin cliente.
     carrito  "CODIGO,cantidad|CODIGO,cantidad|...".
     cmp      Número de Colegiatura Médica (CMP) del recetario. Se exige
              cuando el carrito incluye un producto con requiere_receta y el
              vendedor NO es ADMIN; puede ser "" / NULL en el resto de casos.
     username Usuario que realiza la venta (se resuelve a usuarios.id) o
              NULL para registrar id_vendedor NULL. Debe pertenecer a la
              misma sucursal de la sesión.
     id_sucursal Sucursal de la sesión. Solo permite vender productos de
                 esa sucursal; la venta y el descuento de stock quedan
                 etiquetados con ese id_sucursal.

   Contrato de salida (stdout):
     OK;VENTA;<id_venta>;<total>
     ERROR;<motivo>  (Carrito_Vacio, Carrito_Invalido, DNI_Invalido,
                      Stock_Insuficiente:<codigo>, Producto_No_Encontrado:<codigo>,
                      Receta_Requerida, Vendedor_No_Encontrado,
                      conexion_bd_fallida, ...)

   Devuelve 0 si la venta se confirmo (COMMIT) o -1 si se revirtio. */
int procesarVenta(const char *dni, const char *carrito, const char *cmp,
                  const char *username, int id_sucursal);

#endif
