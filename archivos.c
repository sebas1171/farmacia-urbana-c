#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include "archivos.h"

/* Resuelve la ruta absoluta de farmacia.txt junto al ejecutable.
 * En Linux usa /proc/self/exe; si falla, deja la ruta relativa. */
static void resolverRutaDatos(char *buf, size_t tam) {
    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n > 0) {
        exe[n] = '\0';
        char *dir = dirname(exe);
        snprintf(buf, tam, "%s/%s", dir, ARCHIVO_NOMBRE);
    } else {
        snprintf(buf, tam, "%s", ARCHIVO_NOMBRE);
    }
}

void cargarDatos(Producto inventario[], int *total) {
    char ruta[PATH_MAX];
    resolverRutaDatos(ruta, sizeof(ruta));
    FILE *f = fopen(ruta, "r");
    if (f == NULL) {
        *total = 0;
        return;
    }

    int count = 0;
    char linea[256];
    while (count < MAX_PRODUCTOS && fgets(linea, sizeof(linea), f)) {
        /* Eliminar salto de línea final si existe. */
        size_t len = strlen(linea);
        if (len > 0 && linea[len - 1] == '\n') {
            linea[len - 1] = '\0';
            len--;
        }
        if (len == 0) {
            continue; /* línea vacía: se salta */
        }

        int id;
        int stock;
        float precio;
        char nombre[50];
        char lote[20];
        char fecha_vencimiento[11];

        int leidos = sscanf(linea,
            "%d;%49[^;];%f;%d;%19[^;];%10[^\n]",
            &id, nombre, &precio, &stock, lote, fecha_vencimiento);

        if (leidos < 6) {
            continue; /* línea corrupta: se salta sin abortar */
        }

        Producto *p = &inventario[count];
        p->id = id;
        strncpy(p->nombre, nombre, sizeof(p->nombre) - 1);
        p->nombre[sizeof(p->nombre) - 1] = '\0';
        p->precio = precio;
        p->stock = stock;
        strncpy(p->lote, lote, sizeof(p->lote) - 1);
        p->lote[sizeof(p->lote) - 1] = '\0';
        strncpy(p->fecha_vencimiento, fecha_vencimiento, sizeof(p->fecha_vencimiento) - 1);
        p->fecha_vencimiento[sizeof(p->fecha_vencimiento) - 1] = '\0';

        count++;
    }

    *total = count;
    fclose(f);
}

void guardarDatos(Producto inventario[], int total) {
    char ruta[PATH_MAX];
    resolverRutaDatos(ruta, sizeof(ruta));
    FILE *f = fopen(ruta, "w");
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
