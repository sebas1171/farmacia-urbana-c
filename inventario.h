#ifndef INVENTARIO_H
#define INVENTARIO_H

#include "archivos.h"   /* aporta Producto, MAX_PRODUCTOS y buscarPorId() */

int  registrarProducto(Producto inventario[], int *total, int id,
                       const char *nombre, float precio, int stock,
                       const char *lote, const char *fecha_vencimiento);
int  editarProducto(Producto inventario[], int total, int id,
                    const char *nombre, float precio, int stock,
                    const char *lote, const char *fecha_vencimiento);
void listarProductos(Producto inventario[], int total);

#endif
