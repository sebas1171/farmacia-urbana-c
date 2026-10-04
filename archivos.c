#include <stdio.h>
#include "archivos.h"

void cargarDatos(Producto inventario[], int *total) {
    FILE *f = fopen(ARCHIVO_DATOS, "r");
    if (f == NULL) {
        *total = 0;
        return;
    }

    int count = 0;
    while (count < MAX_PRODUCTOS) {
        int leidos = fscanf(f, "%d;%49[^;];%f;%d;%19[^;];%10[^\n]\n",
            &inventario[count].id,
            inventario[count].nombre,
            &inventario[count].precio,
            &inventario[count].stock,
            inventario[count].lote,
            inventario[count].fecha_vencimiento);

        if (leidos == EOF || leidos < 6) {
            break;
        }
        count++;
    }

    *total = count;
    fclose(f);
}

void guardarDatos(Producto inventario[], int total) {
    FILE *f = fopen(ARCHIVO_DATOS, "w");
    if (f == NULL) {
        return;
    }

    for (int i = 0; i < total; i++) {
        fprintf(f, "%d;%s;%.2f;%d;%s;%s\n",
            inventario[i].id,
            inventario[i].nombre,
            inventario[i].precio,
            inventario[i].stock,
            inventario[i].lote,
            inventario[i].fecha_vencimiento);
    }

    fclose(f);
}

int buscarPorId(Producto inventario[], int total, int id) {
    for (int i = 0; i < total; i++) {
        if (inventario[i].id == id) {
            return i;
        }
    }
    return -1;
}
