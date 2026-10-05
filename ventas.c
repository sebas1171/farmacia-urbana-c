#include <stdio.h>
#include "ventas.h"
#include "archivos.h"

void realizarVenta(Producto inventario[], int total, int id, int cantidad) {
    int idx;
    float monto;

    if (inventario == 0 || total <= 0 || cantidad <= 0) {
        printf("ERROR|CANTIDAD_INVALIDA");
        return;
    }

    idx = buscarPorId(inventario, total, id);
    if (idx < 0) {
        printf("ERROR|ID_NO_EXISTE");
        return;
    }

    if (inventario[idx].stock < cantidad) {
        printf("ERROR|STOCK_INSUFICIENTE");
        return;
    }

    monto = inventario[idx].precio * cantidad;
    inventario[idx].stock -= cantidad;
    guardarDatos(inventario, total);
    printf("EXITO|%.2f", monto);
}
