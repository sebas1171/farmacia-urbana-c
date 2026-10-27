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

/* --- PostgreSQL (libpq): generación de códigos y búsqueda --- */

/* Genera el próximo código de producto con el patrón MED-XXX-NNN a partir
   del id de la categoría. Devuelve 0 y rellena `buffer` con el código, o -1
   si la categoría no existe / no hay conexión. */
int  generarCodigoProducto(int id_categoria, char *buffer, size_t tam);

/* Busca productos por código o nombre (ILIKE) usando parámetros.
   Imprime por stdout el contrato:
     OK;Codigo;Nombre;Precio;Stock;FechaVencimiento
   o ERROR;Producto_No_Encontrado si no hay resultados. */
void buscarProductos(const char *texto);

/* Cierra la conexión PostgreSQL (PQfinish). */
void cerrarConexion(void);

#endif
