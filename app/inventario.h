#ifndef INVENTARIO_H
#define INVENTARIO_H

#include <stddef.h>

#include "archivos.h"   /* aporta Producto, MAX_PRODUCTOS y buscarPorId() */

int  registrarProducto(Producto inventario[], int *total, int id,
                       const char *nombre, float precio, double stock,
                       const char *lote, const char *fecha_vencimiento);
int  editarProducto(Producto inventario[], int total, int id,
                    const char *nombre, float precio, double stock,
                    const char *lote, const char *fecha_vencimiento);
void listarProductos(Producto inventario[], int total);

/* --- PostgreSQL (libpq): generación de códigos y búsqueda --- */

/* Genera el próximo código de producto con el patrón MED-XXX-NNN a partir
   del id de la categoría. Devuelve 0 y rellena `buffer` con el código, o -1
   si la categoría no existe / no hay conexión. */
int  generarCodigoProducto(int id_categoria, char *buffer, size_t tam);

/* Busca productos de UNA sucursal por código o nombre (ILIKE) usando parámetros.
   Imprime por stdout el contrato:
     OK;Codigo;Nombre;Precio;Stock;Vencimiento;Categoria;Requiere_Receta
   El índice 4 de cada fila es siempre la fecha de vencimiento, el 5 la
   categoría y el 6 si el producto requiere receta (1/0). Si no hay
   resultados imprime OK;SIN_RESULTADOS (tabla vacía). */
void buscarProductos(const char *texto, int id_sucursal);

/* Busca un único producto por su código exacto dentro de la sucursal.
   Imprime por stdout el contrato:
     OK;Codigo;Nombre;Precio;Stock;Vencimiento
   o OK;SIN_RESULTADOS si no existe. */
void buscarProductoPorCodigo(const char *codigo, int id_sucursal);

/* --- CRUD real de inventario contra PostgreSQL ---
   Contrato de salida:
     OK;Producto_Creado / OK;Producto_Actualizado / OK;Producto_Eliminado
     ERROR;<motivo>
   lote, fecha_vencimiento y categoria son opcionales (pueden ser NULL o "").
   Si crear_prod recibe codigo == "AUTO" (o vacío), genera un EAN-13 peruano
   de 13 dígitos que comienza con "775". La categoría vacía usa 'Medicamento'.
   Todas las operaciones afectan únicamente a los productos de id_sucursal. */
int crear_prod(const char *codigo, const char *nombre, const char *precio,
               const char *stock, const char *lote, const char *fecha_vencimiento,
               const char *categoria, int id_sucursal);
int editar_prod(const char *codigo, const char *nombre, const char *precio,
                const char *stock, const char *lote, const char *fecha_vencimiento,
                const char *categoria, int id_sucursal);
int eliminar_prod(const char *codigo, int id_sucursal);

/* Cierra la conexión PostgreSQL (PQfinish). */
void cerrarConexion(void);

#endif
