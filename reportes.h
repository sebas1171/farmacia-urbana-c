#ifndef REPORTES_H
#define REPORTES_H

#include "archivos.h"

/* Umbral por defecto para el reporte de stock mínimo.
   Consistente con app.py (Producto.stock_bajo) y stub_alertas en main.c. */
#define STOCK_MINIMO_DEFAULT 10

/* Imprime los productos cuyo stock es estrictamente menor que `umbral`.
   Formato de salida (una línea por producto, sin encabezado):
       ID;Nombre;Stock;FechaVencimiento

   El inventario se recibe por parámetro: esta función no abre archivos
   ni modifica los datos. Devuelve la cantidad de filas impresas; stdout
   queda como data cruda para el consumidor (interfaz Python). */
int reporteStockMinimo(const Producto inventario[], int total, int umbral);

/* Imprime los productos cuya fecha_vencimiento es menor o igual que
   `fecha_limite` (formato ISO "YYYY-MM-DD"). Como el formato es ISO 8601,
   la comparación lexicográfica equivale al orden cronológico.

   Formato de salida (una línea por producto, sin encabezado):
       ID;Nombre;Stock;FechaVencimiento

   Si `fecha_limite` no tiene formato válido, no imprime nada.
   Devuelve la cantidad de filas impresas. */
int reporteVencimientos(const Producto inventario[], int total, const char *fecha_limite);

#endif
