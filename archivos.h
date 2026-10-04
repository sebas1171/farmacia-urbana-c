#ifndef ARCHIVOS_H
#define ARCHIVOS_H

#define MAX_PRODUCTOS 100
#define ARCHIVO_DATOS "farmacia.txt"

typedef struct {
    int id;
    char nombre[50];
    float precio;
    int stock;
    char lote[20];
    char fecha_vencimiento[11]; /* Formato YYYY-MM-DD */
} Producto;

void cargarDatos(Producto inventario[], int *total);
void guardarDatos(Producto inventario[], int total);
int buscarPorId(Producto inventario[], int total, int id);

#endif
