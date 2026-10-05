#include <stdio.h>
#include <string.h>

#include "reportes.h"

/* Verifica que `fecha` tenga el formato ISO "YYYY-MM-DD":
   - 10 caracteres exactos
   - dígitos en las posiciones 0-3, 5-6 y 8-9
   - guiones en las posiciones 4 y 7 */
static int fechaValida(const char *fecha) {
    if (fecha == NULL || strlen(fecha) != 10) {
        return 0;
    }
    if (fecha[4] != '-' || fecha[7] != '-') {
        return 0;
    }
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            continue;
        }
        if (fecha[i] < '0' || fecha[i] > '9') {
            return 0;
        }
    }
    return 1;
}

int reporteStockMinimo(const Producto inventario[], int total, int umbral) {
    if (inventario == NULL || total <= 0) {
        return 0;
    }

    int impresos = 0;
    for (int i = 0; i < total; i++) {
        if (inventario[i].stock < umbral) {
            printf("%d;%s;%d;%s\n",
                inventario[i].id,
                inventario[i].nombre,
                inventario[i].stock,
                inventario[i].fecha_vencimiento);
            impresos++;
        }
    }
    return impresos;
}

int reporteVencimientos(const Producto inventario[], int total, const char *fecha_limite) {
    if (inventario == NULL || total <= 0 || !fechaValida(fecha_limite)) {
        return 0;
    }

    int impresos = 0;
    for (int i = 0; i < total; i++) {
        if (!fechaValida(inventario[i].fecha_vencimiento)) {
            continue;  /* fecha malformada: se omite para no comparar en falso */
        }
        if (strcmp(inventario[i].fecha_vencimiento, fecha_limite) <= 0) {
            printf("%d;%s;%d;%s\n",
                inventario[i].id,
                inventario[i].nombre,
                inventario[i].stock,
                inventario[i].fecha_vencimiento);
            impresos++;
        }
    }
    return impresos;
}
