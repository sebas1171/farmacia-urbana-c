#ifndef VENTAS_H
#define VENTAS_H

// Función para buscar producto por código
void buscar_producto_venta(const char* codigo);

// Función para procesar la compra (descontar stock y registrar)
void procesar_venta(const char* codigo, int cantidad);

#endif
