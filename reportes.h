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

/* Imprime el historial de ventas del periodo [fecha_inicio, fecha_fin]
   (ambas en formato ISO "YYYY-MM-DD") y, opcionalmente, filtrado por vendedor y/o cliente (DNI).

   Una linea por detalle de venta:

       OK;FILA;<fecha_hora>;<vendedor>;<producto>;<cantidad>;<subtotal>

   Si no se proporciona un ID de vendedor valido (NULL o vacio), se muestran
   las ventas de todos los vendedores.

   Si no se proporciona un DNI de cliente valido (NULL o vacio), no se
   aplica el filtro por cliente. Si se proporciona, debe tener 8 digitos.

   Si el periodo no tiene ventas:
       OK;SIN_RESULTADOS

   Si las fechas son invalidas o el rango es incorrecto:
       ERROR;<motivo>

   Devuelve la cantidad de filas impresas (0 si no hay resultados). */
int reporteHistorialVentas(const char *fecha_inicio, const char *fecha_fin, const char *id_vendedor, const char *dni_cliente);

/* Motor de alertas: imprime el resumen de alertas.

   Formato:
       OK;ALERTAS;BAJO_STOCK=<n_productos_stock_critico>;POR_VENCER=<n_productos_por_vencer>

   BAJO_STOCK: cantidad de productos con stock <= STOCK_MINIMO_DEFAULT (10).
   POR_VENCER: cantidad de productos con fecha_vencimiento >= CURRENT_DATE y
              < CURRENT_DATE + INTERVAL '30 days' (vencen en menos de 30 dias).

   Devuelve 0 si la consulta fue exitosa, -1 si no lo fue. */
int reporteAlertas(void);
int reporteAlertasStock(void);
int reporteAlertasVencimientos(void);

/* Imprime el producto mas vendido en el periodo [fecha_inicio, fecha_fin].

   Formato:
       OK;PRODUCTO_MAS_VENDIDO;<codigo>;<nombre>;<cantidad>

   Si no hay ventas en el periodo:
       OK;SIN_RESULTADOS

   Devuelve 0 si la consulta fue exitosa, -1 si no lo fue. */
int reporteProductoMasVendido(const char *fecha_inicio, const char *fecha_fin);

/* Imprime los ingresos totales del periodo [fecha_inicio, fecha_fin].

   Formato:
       OK;INGRESOS_TOTALES;<total>

   Los ingresos se calculan con SUM(detalle_ventas.subtotal) y COALESCE para
   evitar errores cuando no hay ventas/detalles.

   Devuelve 0 si la consulta fue exitosa, -1 si no lo fue. */
int reporteIngresosTotales(const char *fecha_inicio, const char *fecha_fin);

#endif
