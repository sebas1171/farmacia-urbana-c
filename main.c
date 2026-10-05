#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "archivos.h"
#include "inventario.h"

static void stub_login(const char *usuario, const char *password) {
    if (usuario == NULL || password == NULL) {
        printf("ERROR;parametros_faltantes\n");
        return;
    }
    if (strcmp(usuario, "admin") == 0 && strcmp(password, "1234") == 0) {
        printf("OK;login_correcto\n");
    } else {
        printf("ERROR;credenciales_invalidas\n");
    }
}

static void stub_vender(Producto inventario[], int total, const char *idStr, const char *cantStr) {
    if (idStr == NULL || cantStr == NULL) {
        printf("ERROR;parametros_faltantes\n");
        return;
    }
    int id = atoi(idStr);
    int cantidad = atoi(cantStr);
    if (id <= 0 || cantidad <= 0) {
        printf("ERROR;parametros_invalidos\n");
        return;
    }
    int idx = buscarPorId(inventario, total, id);
    if (idx < 0) {
        printf("ERROR;producto_no_encontrado\n");
        return;
    }
    if (inventario[idx].stock < cantidad) {
        printf("ERROR;stock_insuficiente\n");
        return;
    }
    inventario[idx].stock -= cantidad;
    printf("OK;venta;id=%d;cantidad=%d;stock_restante=%d\n",
        id, cantidad, inventario[idx].stock);
}

static void stub_alertas(Producto inventario[], int total) {
    int n = 0;
    for (int i = 0; i < total; i++) {
        if (inventario[i].stock < 10) {
            n++;
        }
    }
    printf("OK;alertas=%d\n", n);
    for (int i = 0; i < total; i++) {
        if (inventario[i].stock < 10) {
            printf("ALERTA;%d;%s;%d\n",
                inventario[i].id,
                inventario[i].nombre,
                inventario[i].stock);
        }
    }
}

int main(int argc, char *argv[]) {
    Producto inventario[MAX_PRODUCTOS];
    int total = 0;

    cargarDatos(inventario, &total);

    if (argc < 2) {
        printf("ERROR;uso_invalido\n");
        return 1;
    }

    if (strcmp(argv[1], "login") == 0) {
        stub_login(argc > 2 ? argv[2] : NULL, argc > 3 ? argv[3] : NULL);
    } else if (strcmp(argv[1], "listar") == 0) {
        listarProductos(inventario, total);
    } else if (strcmp(argv[1], "registrar") == 0) {
        if (argc != 8) {
            printf("ERROR;uso_invalido\n");
            return 1;
        }
        char *end = NULL;
        errno = 0;
        long id_l = strtol(argv[2], &end, 10);
        if (errno != 0 || end == argv[2] || *end != '\0') {
            printf("ERROR;parametros_invalidos\n");
            return 1;
        }
        end = NULL;
        errno = 0;
        float precio = strtof(argv[4], &end);
        if (errno != 0 || end == argv[4] || *end != '\0') {
            printf("ERROR;parametros_invalidos\n");
            return 1;
        }
        end = NULL;
        errno = 0;
        long stock_l = strtol(argv[5], &end, 10);
        if (errno != 0 || end == argv[5] || *end != '\0') {
            printf("ERROR;parametros_invalidos\n");
            return 1;
        }
        int id = (int)id_l;
        int stock = (int)stock_l;
        int r = registrarProducto(inventario, &total, id, argv[3], precio, stock, argv[6], argv[7]);
        if (r == 0) {
            printf("OK;registrado;id=%d\n", id);
        } else {
            printf("ERROR;registro_invalido\n");
            return 1;
        }
    } else if (strcmp(argv[1], "editar") == 0) {
        if (argc != 8) {
            printf("ERROR;uso_invalido\n");
            return 1;
        }
        char *end = NULL;
        errno = 0;
        long id_l = strtol(argv[2], &end, 10);
        if (errno != 0 || end == argv[2] || *end != '\0') {
            printf("ERROR;parametros_invalidos\n");
            return 1;
        }
        end = NULL;
        errno = 0;
        float precio = strtof(argv[4], &end);
        if (errno != 0 || end == argv[4] || *end != '\0') {
            printf("ERROR;parametros_invalidos\n");
            return 1;
        }
        end = NULL;
        errno = 0;
        long stock_l = strtol(argv[5], &end, 10);
        if (errno != 0 || end == argv[5] || *end != '\0') {
            printf("ERROR;parametros_invalidos\n");
            return 1;
        }
        int id = (int)id_l;
        int stock = (int)stock_l;
        int r = editarProducto(inventario, total, id, argv[3], precio, stock, argv[6], argv[7]);
        if (r == 0) {
            printf("OK;editado;id=%d\n", id);
        } else {
            printf("ERROR;edicion_invalida\n");
            return 1;
        }
    } else if (strcmp(argv[1], "vender") == 0) {
        stub_vender(inventario, total, argc > 2 ? argv[2] : NULL, argc > 3 ? argv[3] : NULL);
    } else if (strcmp(argv[1], "alertas") == 0) {
        stub_alertas(inventario, total);
    } else {
        printf("ERROR;comando_no_reconocido\n");
        return 1;
    }

    guardarDatos(inventario, total);
    return 0;
}
