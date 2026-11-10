#ifndef VENTAS_H
#define VENTAS_H

// Solo firmas, sin implementaciones.
// Las guardas #ifndef evitan errores de doble inclusion en el linker.

void buscar_producto_venta(const char* codigo);
void procesar_venta(const char* codigo, int cantidad, int id_vendedor, const char* dni_cliente);

#endif
