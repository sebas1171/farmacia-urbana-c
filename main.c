#include <stdio.h>
#include <string.h>

#include "reportes.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("ERROR;Comando_vacio\n");
        return 1;
    }

    // Simulador de Andrea (Login)
    if (strcmp(argv[1], "login") == 0) {
        printf("OK;ADMINISTRADOR;Sebastian\n");
    }
    // Simulador de Dariana (Inventario)
    else if (strcmp(argv[1], "buscar_prod") == 0) {
        printf("OK;MED-001;Paracetamol;5.50;100;2027-12-01\n");
        printf("OK;MED-002;Ibuprofeno;7.20;50;2028-05-15\n");
    }
    // Motor de Alertas (Reportes)
    else if (strcmp(argv[1], "alertas") == 0) {
        if (reporteAlertas() != 0) {
            return 1;
        }
    }
    // Simulador de Usuarios
    else if (strcmp(argv[1], "listar_usuarios") == 0) {
        printf("OK;72345678;Sebastian;admin;ADMINISTRADOR\n");
        printf("OK;71112222;Paolo;pvendedor;VENDEDOR\n");
    }
    else if (strcmp(argv[1], "crear_usuario") == 0 || strcmp(argv[1], "editar_usuario") == 0 || strcmp(argv[1], "eliminar_usuario") == 0) {
        printf("OK;Operacion_Exitosa\n");
    }
    // Simulador de Paolo (Ventas)
    else if (strcmp(argv[1], "buscar_prod_codigo") == 0) {
        printf("OK;Paracetamol;5.50;100\n");
    }
    else if (strcmp(argv[1], "procesar_venta") == 0) {
        printf("OK;Venta_Guardada_Con_Exito\n");
    }
    // Simulador de Azumi (Reportes)
    else if (strcmp(argv[1], "historial") == 0) {
        char fecha_inicio[32] = "";
        char fecha_fin[32] = "";
        char id_vendedor[32] = "";
        if (argc > 2) {
            snprintf(fecha_inicio, sizeof(fecha_inicio), "%s", argv[2]);
        }
        if (argc > 3) {
            snprintf(fecha_fin, sizeof(fecha_fin), "%s", argv[3]);
        }
        if (argc > 4) {
            snprintf(id_vendedor, sizeof(id_vendedor), "%s", argv[4]);
        }
        if (reporteHistorialVentas(fecha_inicio, fecha_fin, id_vendedor) < 0) {
            return 1;
        }
    }
    // Metrica: Producto Mas Vendido (Reportes)
    else if (strcmp(argv[1], "producto_mas_vendido") == 0) {
        char fecha_inicio[32] = "";
        char fecha_fin[32] = "";
        if (argc > 2) {
            snprintf(fecha_inicio, sizeof(fecha_inicio), "%s", argv[2]);
        }
        if (argc > 3) {
            snprintf(fecha_fin, sizeof(fecha_fin), "%s", argv[3]);
        }
        if (reporteProductoMasVendido(fecha_inicio, fecha_fin) != 0) {
            return 1;
        }
    }
    // Metrica: Ingresos Totales (Reportes)
    else if (strcmp(argv[1], "ingresos_totales") == 0) {
        char fecha_inicio[32] = "";
        char fecha_fin[32] = "";
        if (argc > 2) {
            snprintf(fecha_inicio, sizeof(fecha_inicio), "%s", argv[2]);
        }
        if (argc > 3) {
            snprintf(fecha_fin, sizeof(fecha_fin), "%s", argv[3]);
        }
        if (reporteIngresosTotales(fecha_inicio, fecha_fin) != 0) {
            return 1;
        }
    }
    // Simulador del Chatbot
    else if (strcmp(argv[1], "chatbot") == 0) {
        printf("OK;Hola, la conexion entre Python y C esta funcionando a la perfeccion.\n");
    }
    else {
        printf("ERROR;Comando_no_reconocido\n");
    }

    return 0;
}
