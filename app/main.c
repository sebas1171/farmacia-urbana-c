#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "reportes.h"
#include "usuarios.h"
#include "inventario.h"
#include "ventas.h"

/* Convierte un argumento a entero positivo. Devuelve 0 si es invalido. */
static int parsearId(const char *texto) {
    if (texto == NULL || texto[0] == '\0') {
        return 0;
    }
    errno = 0;
    char *fin = NULL;
    long valor = strtol(texto, &fin, 10);
    if (errno == ERANGE || fin == texto || *fin != '\0' ||
        valor <= 0 || valor > INT_MAX) {
        return 0;
    }
    return (int)valor;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("ERROR;Comando_vacio\n");
        return 1;
    }

    // Simulador de Andrea (Login)
    if (strcmp(argv[1], "login") == 0) {
        printf("OK;ADMINISTRADOR;Sebastian\n");
    }
    // Inventario: busqueda real en PostgreSQL (alimenta el autocomplete de Ventas)
    else if (strcmp(argv[1], "buscar_prod") == 0) {
        buscarProductos(argc > 2 ? argv[2] : "");
    }
    // Motor de Alertas (Reportes)
    else if (strcmp(argv[1], "alertas") == 0) {
        if (reporteAlertas() != 0) {
            return 1;
        }
    }
    else if (strcmp(argv[1], "alertas_stock") == 0) {
        if (reporteAlertasStock() != 0) {
            return 1;
        }
    }
    else if (strcmp(argv[1], "alertas_vencimientos") == 0) {
        if (reporteAlertasVencimientos() != 0) {
            return 1;
        }
    }
    // Usuarios (CRUD real contra PostgreSQL, tabla `usuarios`)
    else if (strcmp(argv[1], "listar_usuarios") == 0) {
        listarUsuarios();
    }
    else if (strcmp(argv[1], "crear_usuario") == 0) {
        /* Formato vigente: crear_usuario <nombre> <username> <password> <rol>
           Formato legado: crear_usuario <dni_ignorado> <nombre> <username> <password> <rol> */
        if (argc >= 7) {
            crearUsuario(argv[3], argv[4], argv[5], argv[6]);
        } else if (argc >= 6) {
            crearUsuario(argv[2], argv[3], argv[4], argv[5]);
        } else {
            printf("ERROR;Parametros_Invalidos\n");
            return 1;
        }
    }
    else if (strcmp(argv[1], "editar_usuario") == 0) {
        /* Formato vigente: editar_usuario <id> <rol> */
        if (argc < 4) {
            printf("ERROR;Parametros_Invalidos\n");
            return 1;
        }
        int id = parsearId(argv[2]);
        if (id == 0) {
            printf("ERROR;ID_Invalido\n");
            return 1;
        }
        editarUsuario(id, argv[3]);
    }
    else if (strcmp(argv[1], "eliminar_usuario") == 0) {
        /* Formato vigente: eliminar_usuario <id> */
        if (argc < 3) {
            printf("ERROR;Parametros_Invalidos\n");
            return 1;
        }
        int id = parsearId(argv[2]);
        if (id == 0) {
            printf("ERROR;ID_Invalido\n");
            return 1;
        }
        eliminarUsuario(id);
    }
    // Ventas: registro real en PostgreSQL dentro de una transaccion
    else if (strcmp(argv[1], "buscar_prod_codigo") == 0) {
        buscarProductos(argc > 2 ? argv[2] : "");
    }
    else if (strcmp(argv[1], "procesar_venta") == 0) {
        if (argc < 4) {
            printf("ERROR;Parametros_Invalidos\n");
            return 1;
        }
        const char *username = argc > 4 ? argv[4] : getenv("FARMACIA_USER");
        if (procesarVenta(argv[2], argv[3], username) != 0) {
            return 1;
        }
    }
    // Simulador de Azumi (Reportes)
    else if (strcmp(argv[1], "historial") == 0) {
        char fecha_inicio[32] = "";
        char fecha_fin[32] = "";
        char id_vendedor[32] = "";
        char dni_cliente[16] = "";

        if (argc > 2) {
            snprintf(fecha_inicio, sizeof(fecha_inicio), "%s", argv[2]);
        }
        if (argc > 3) {
            snprintf(fecha_fin, sizeof(fecha_fin), "%s", argv[3]);
        }
        if (argc > 4 && strcmp(argv[4], "-") != 0) {
            snprintf(id_vendedor, sizeof(id_vendedor), "%s", argv[4]);
        }
        if (argc > 5) {
            snprintf(dni_cliente, sizeof(dni_cliente), "%s", argv[5]);
        }

        if (reporteHistorialVentas(fecha_inicio, fecha_fin,
                                   id_vendedor, dni_cliente) < 0) {
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
