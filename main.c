#include <stdio.h>
#include <string.h>

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
    // Simulador de Alertas
    else if (strcmp(argv[1], "alertas") == 0) {
        printf("OK;CRITICO;MED-003;Amoxicilina;2\n");
        printf("OK;VENCE;MED-002;Ibuprofeno;2026-11-05\n");
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
        printf("OK;2026-10-28;Paolo;Paracetamol;2;11.00\n");
        printf("OK;2026-10-28;Andrea;Ibuprofeno;1;7.20\n");
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
