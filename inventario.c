#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "inventario.h"

/* Recorre la cadena y rechaza comas y saltos de línea (rompen el CSV). */
static int textoLimpio(const char *s) {
    if (s == NULL) {
        return 0;
    }
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] == ',' || s[i] == '\n' || s[i] == '\r') {
            return 0;
        }
    }
    return 1;
}

/* Formato estricto YYYY-MM-DD: 10 chars, dígitos y guiones en su sitio. */
static int fechaValida(const char *f) {
    if (f == NULL || strlen(f) != sizeof(((Producto *)0)->fecha_vencimiento) - 1) {
        return 0;
    }
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            if (f[i] != '-') {
                return 0;
            }
        } else if (!isdigit((unsigned char)f[i])) {
            return 0;
        }
    }
    return 1;
}

/* Valida los campos comunes. Devuelve 1 si todo es correcto. */
static int camposValidos(const char *nombre, float precio, int stock,
                         const char *lote, const char *fecha_vencimiento) {
    if (nombre == NULL || lote == NULL || fecha_vencimiento == NULL) {
        return 0;
    }
    if (strlen(nombre) == 0 || strlen(lote) == 0 || strlen(fecha_vencimiento) == 0) {
        return 0;
    }
    if (strlen(nombre) >= sizeof(((Producto *)0)->nombre) ||
        strlen(lote)   >= sizeof(((Producto *)0)->lote)) {
        return 0;
    }
    if (precio < 0.0f || stock < 0) {
        return 0;
    }
    if (!textoLimpio(nombre) || !textoLimpio(lote) || !textoLimpio(fecha_vencimiento)) {
        return 0;
    }
    if (!fechaValida(fecha_vencimiento)) {
        return 0;
    }
    return 1;
}

/* Copia segura forzando terminador '\0'. */
static void copiarCampo(char *dest, size_t tam, const char *src) {
    strncpy(dest, src, tam - 1);
    dest[tam - 1] = '\0';
}

int registrarProducto(Producto inventario[], int *total, int id,
                      const char *nombre, float precio, int stock,
                      const char *lote, const char *fecha_vencimiento) {
    if (inventario == NULL || total == NULL) {
        return -1;
    }
    if (*total < 0 || *total >= MAX_PRODUCTOS) {
        return -1;
    }
    if (id <= 0) {
        return -1;
    }
    if (!camposValidos(nombre, precio, stock, lote, fecha_vencimiento)) {
        return -1;
    }
    if (buscarPorId(inventario, *total, id) != -1) {
        return -1;
    }

    Producto *p = &inventario[*total];
    p->id = id;
    copiarCampo(p->nombre, sizeof(p->nombre), nombre);
    p->precio = precio;
    p->stock = stock;
    copiarCampo(p->lote, sizeof(p->lote), lote);
    copiarCampo(p->fecha_vencimiento, sizeof(p->fecha_vencimiento), fecha_vencimiento);
    (*total)++;
    return 0;
}

int editarProducto(Producto inventario[], int total, int id,
                   const char *nombre, float precio, int stock,
                   const char *lote, const char *fecha_vencimiento) {
    if (inventario == NULL) {
        return -1;
    }
    if (total < 0 || total > MAX_PRODUCTOS) {
        return -1;
    }
    if (id <= 0) {
        return -1;
    }
    if (!camposValidos(nombre, precio, stock, lote, fecha_vencimiento)) {
        return -1;
    }

    int idx = buscarPorId(inventario, total, id);
    if (idx < 0) {
        return -1;
    }

    Producto *p = &inventario[idx];
    copiarCampo(p->nombre, sizeof(p->nombre), nombre);
    p->precio = precio;
    p->stock = stock;
    copiarCampo(p->lote, sizeof(p->lote), lote);
    copiarCampo(p->fecha_vencimiento, sizeof(p->fecha_vencimiento), fecha_vencimiento);
    return 0;
}

void listarProductos(Producto inventario[], int total) {
    if (inventario == NULL || total <= 0) {
        return;
    }
    for (int i = 0; i < total; i++) {
        printf("%d,%s,%.2f,%d,%s,%s\n",
               inventario[i].id,
               inventario[i].nombre,
               inventario[i].precio,
               inventario[i].stock,
               inventario[i].lote,
               inventario[i].fecha_vencimiento);
    }
}
