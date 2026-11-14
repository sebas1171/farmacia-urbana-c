#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "reportes.h"
#include "usuarios.h"
#include "inventario.h"
#include "ventas.h"
#include "login_2.h"
#include "chat_bot.h"
#include "auditoria.h"

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

/* Lee y valida el id_sucursal obligatorio de una petición IPC.
   Devuelve 1 y deja el valor en *destino, o 0 si falta / es inválido. */
static int exigirSucursal(int argc, char *argv[], int indice, int *destino) {
    if (indice >= argc) {
        printf("ERROR;Parametros_Invalidos\n");
        return 0;
    }
    int id = parsearId(argv[indice]);
    if (id == 0) {
        printf("ERROR;Sucursal_Invalida\n");
        return 0;
    }
    *destino = id;
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("ERROR;Comando_vacio\n");
        return 1;
    }

    const char *comando = argv[1];

    /* Login real contra PostgreSQL: OK;<rol>;<nombre>. */
    if (strcmp(comando, "login") == 0) {
        if (argc < 4) {
            printf("ERROR;Parametros_Invalidos\n");
            return 1;
        }
        validar_login(argv[2], argv[3]);
    }
    /* Inventario: busqueda real en PostgreSQL (alimenta autocomplete y tabla). */
    else if (strcmp(comando, "buscar_prod") == 0) {
        int sucursal = 0;
        if (!exigirSucursal(argc, argv, 3, &sucursal)) {
            return 1;
        }
        buscarProductos(argc > 2 ? argv[2] : "", sucursal);
    }
    else if (strcmp(comando, "buscar_prod_codigo") == 0) {
        int sucursal = 0;
        if (argc < 3 || !exigirSucursal(argc, argv, 3, &sucursal)) {
            printf("ERROR;Parametros_Invalidos\n");
            return 1;
        }
        buscarProductoPorCodigo(argv[2], sucursal);
    }
    /* CRUD real de inventario (solo ADMIN en la interfaz). */
    else if (strcmp(comando, "crear_prod") == 0) {
        /* Formato: crear_prod <codigo> <nombre> <precio> <stock> <lote>
                           <fecha> <categoria> <id_sucursal> */
        int sucursal = 0;
        if (argc < 10 || !exigirSucursal(argc, argv, 9, &sucursal)) {
            printf("ERROR;Parametros_Invalidos\n");
            return 1;
        }
        if (crear_prod(argv[2], argv[3], argv[4], argv[5],
                       argv[6], argv[7], argv[8], sucursal) != 0) {
            return 1;
        }
    }
    else if (strcmp(comando, "editar_prod") == 0) {
        /* Formato: editar_prod <codigo> <nombre> <precio> <stock> <lote>
                           <fecha> <categoria> <id_sucursal> */
        int sucursal = 0;
        if (argc < 10 || !exigirSucursal(argc, argv, 9, &sucursal)) {
            printf("ERROR;Parametros_Invalidos\n");
            return 1;
        }
        if (editar_prod(argv[2], argv[3], argv[4], argv[5],
                        argv[6], argv[7], argv[8], sucursal) != 0) {
            return 1;
        }
    }
    else if (strcmp(comando, "eliminar_prod") == 0) {
        int sucursal = 0;
        if (argc < 3 || !exigirSucursal(argc, argv, 3, &sucursal)) {
            printf("ERROR;Parametros_Invalidos\n");
            return 1;
        }
        if (eliminar_prod(argv[2], sucursal) != 0) {
            return 1;
        }
    }
    /* Motor de Alertas (Reportes). */
    else if (strcmp(comando, "alertas") == 0) {
        int sucursal = 0;
        if (!exigirSucursal(argc, argv, 2, &sucursal)) {
            return 1;
        }
        if (reporteAlertas(sucursal) != 0) {
            return 1;
        }
    }
    else if (strcmp(comando, "alertas_stock") == 0) {
        int sucursal = 0;
        if (!exigirSucursal(argc, argv, 2, &sucursal)) {
            return 1;
        }
        if (reporteAlertasStock(sucursal) != 0) {
            return 1;
        }
    }
    else if (strcmp(comando, "alertas_vencimientos") == 0) {
        int sucursal = 0;
        if (!exigirSucursal(argc, argv, 2, &sucursal)) {
            return 1;
        }
        if (reporteAlertasVencimientos(sucursal) != 0) {
            return 1;
        }
    }
    /* Usuarios (CRUD real contra PostgreSQL, tabla `usuarios`). */
    else if (strcmp(comando, "listar_sucursales") == 0) {
        listarSucursales();
    }
    else if (strcmp(comando, "listar_usuarios") == 0) {
        int sucursal = 0;
        if (!exigirSucursal(argc, argv, 2, &sucursal)) {
            return 1;
        }
        listarUsuarios(sucursal);
    }
    else if (strcmp(comando, "crear_usuario") == 0) {
        /* Formato vigente: crear_usuario <nombre> <username> <password> <rol>
                           <id_sucursal> */
        int sucursal = 0;
        if (argc < 7 || !exigirSucursal(argc, argv, 6, &sucursal)) {
            printf("ERROR;Parametros_Invalidos\n");
            return 1;
        }
        crearUsuario(argv[2], argv[3], argv[4], argv[5], sucursal);
    }
    else if (strcmp(comando, "editar_usuario") == 0) {
        /* Formato vigente: editar_usuario <id> <rol> <id_sucursal> */
        int sucursal = 0;
        if (argc < 5 || !exigirSucursal(argc, argv, 4, &sucursal)) {
            printf("ERROR;Parametros_Invalidos\n");
            return 1;
        }
        int id = parsearId(argv[2]);
        if (id == 0) {
            printf("ERROR;ID_Invalido\n");
            return 1;
        }
        editarUsuario(id, argv[3], sucursal);
    }
    else if (strcmp(comando, "eliminar_usuario") == 0) {
        /* Formato vigente: eliminar_usuario <id> [id_sucursal]
           La baja es administrativa y global; el id_sucursal se acepta para
           respetar el contrato de sesión. */
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
    /* Ventas: registro real en PostgreSQL dentro de una transaccion.
       Formato: procesar_venta <dni> <carrito> <cmp> <id_sucursal>
       El CMP puede ser "" (sin receta o vendedor ADMIN). */
    else if (strcmp(comando, "procesar_venta") == 0) {
        int sucursal = 0;
        if (argc < 5 || !exigirSucursal(argc, argv, 5, &sucursal)) {
            printf("ERROR;Parametros_Invalidos\n");
            return 1;
        }
        const char *cmp = argv[4];
        const char *username = getenv("FARMACIA_USER");
        if (procesarVenta(argv[2], argv[3], cmp, username, sucursal) != 0) {
            return 1;
        }
    }
    /* Historial de ventas: fechas vacias = reporte abierto (todas).
       Formato: historial <ini> <fin> <id_sucursal>
       Tambien admite los filtros opcionales: <ini> <fin> <vend> <dni> <sucursal>. */
    else if (strcmp(comando, "historial") == 0) {
        /* El id_sucursal siempre es el ultimo argumento:
           [ini] [fin] [vend] [dni] [sucursal]. */
        int indice_sucursal = argc >= 6 ? argc - 1 : 4;
        int sucursal = 0;
        if (argc < 5 || !exigirSucursal(argc, argv, indice_sucursal, &sucursal)) {
            printf("ERROR;Parametros_Invalidos\n");
            return 1;
        }
        const char *fecha_inicio = argc > 2 ? argv[2] : "";
        const char *fecha_fin = argc > 3 ? argv[3] : "";
        const char *id_vendedor =
            (argc >= 6 && strcmp(argv[4], "-") != 0) ? argv[4] : "";
        const char *dni_cliente = argc >= 7 ? argv[5] : "";

        if (reporteHistorialVentas(fecha_inicio, fecha_fin,
                                   id_vendedor, dni_cliente, sucursal) < 0) {
            return 1;
        }
    }
    /* Metrica: Producto Mas Vendido (Reportes). */
    else if (strcmp(comando, "producto_mas_vendido") == 0) {
        int sucursal = 0;
        if (argc < 4 || !exigirSucursal(argc, argv, 4, &sucursal)) {
            printf("ERROR;Parametros_Invalidos\n");
            return 1;
        }
        const char *fecha_inicio = argc > 2 ? argv[2] : "";
        const char *fecha_fin = argc > 3 ? argv[3] : "";
        if (reporteProductoMasVendido(fecha_inicio, fecha_fin, sucursal) != 0) {
            return 1;
        }
    }
    /* Metrica: Ingresos Totales (Reportes). */
    else if (strcmp(comando, "ingresos_totales") == 0) {
        int sucursal = 0;
        if (argc < 4 || !exigirSucursal(argc, argv, 4, &sucursal)) {
            printf("ERROR;Parametros_Invalidos\n");
            return 1;
        }
        const char *fecha_inicio = argc > 2 ? argv[2] : "";
        const char *fecha_fin = argc > 3 ? argv[3] : "";
        if (reporteIngresosTotales(fecha_inicio, fecha_fin, sucursal) != 0) {
            return 1;
        }
    }
    /* Chatbot real: FAQ del asistente virtual + stock/caducidad en PostgreSQL. */
    else if (strcmp(comando, "chatbot") == 0) {
        int sucursal = 0;
        if (argc < 3 || !exigirSucursal(argc, argv, 3, &sucursal)) {
            printf("ERROR;Pregunta_Vacia\n");
            return 1;
        }
        procesar_chatbot(argv[2], sucursal);
    }
    /* Bitácora de seguridad desde el frontend (ej. cancelación de receta).
       Formato: log_seguridad <mensaje> */
    else if (strcmp(comando, "log_seguridad") == 0) {
        if (argc < 3 || argv[2][0] == '\0') {
            printf("ERROR;Mensaje_Vacio\n");
            return 1;
        }
        escribir_log_local(argv[2]);
        printf("OK;Log_Registrado\n");
    }
    else {
        printf("ERROR;Comando_no_reconocido\n");
        return 1;
    }

    return 0;
}
