#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <libpq-fe.h>

#include "ventas.h"

#define MAX_ITEMS_VENTA 100

typedef struct {
    char codigo[16];
    int cantidad;
} ItemVenta;

static char *trim(char *s) {
    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
    return s;
}

/* Parsea "CODIGO,cantidad|CODIGO,cantidad|...".
   Devuelve el numero de items (>0), 0 si el carrito esta vacio, -1 si el
   formato es invalido. */
static int parsearCarrito(const char *carrito, ItemVenta items[], int max) {
    if (carrito == NULL || carrito[0] == '\0') {
        return 0;
    }

    char *copia = strdup(carrito);
    if (copia == NULL) {
        return -1;
    }

    int n = 0;
    char *tok = strtok(copia, "|");
    while (tok != NULL) {
        char *item = trim(tok);
        char *coma = strchr(item, ',');
        if (coma == NULL) {
            free(copia);
            return -1;
        }
        *coma = '\0';

        char *codigo = trim(item);
        char *cantidad_txt = trim(coma + 1);
        if (codigo[0] == '\0' || cantidad_txt[0] == '\0' ||
            strlen(codigo) >= sizeof(items[0].codigo)) {
            free(copia);
            return -1;
        }

        errno = 0;
        char *fin = NULL;
        long cantidad = strtol(cantidad_txt, &fin, 10);
        if (errno == ERANGE || fin == cantidad_txt || *fin != '\0' ||
            cantidad <= 0 || cantidad > INT_MAX) {
            free(copia);
            return -1;
        }

        int encontrado = -1;
        for (int i = 0; i < n; i++) {
            if (strcmp(items[i].codigo, codigo) == 0) {
                encontrado = i;
                break;
            }
        }
        if (encontrado >= 0) {
            if (items[encontrado].cantidad > INT_MAX - (int)cantidad) {
                free(copia);
                return -1;
            }
            items[encontrado].cantidad += (int)cantidad;
        } else {
            if (n >= max) {
                free(copia);
                return -1;
            }
            snprintf(items[n].codigo, sizeof(items[n].codigo), "%s", codigo);
            items[n].cantidad = (int)cantidad;
            n++;
        }

        tok = strtok(NULL, "|");
    }

    free(copia);
    return n;
}

static int dniValido(const char *dni) {
    if (dni == NULL || dni[0] == '\0') {
        return 1; /* opcional: se guarda NULL */
    }
    if (strlen(dni) != 8) {
        return 0;
    }
    for (int i = 0; i < 8; i++) {
        if (!isdigit((unsigned char)dni[i])) {
            return 0;
        }
    }
    return 1;
}

static void rollbackYError(PGconn *conn, const char *mensaje) {
    PQexec(conn, "ROLLBACK");
    printf("ERROR;%s\n", mensaje);
}

int procesarVenta(const char *dni, const char *carrito, const char *username) {
    if (!dniValido(dni)) {
        printf("ERROR;DNI_Invalido\n");
        return -1;
    }

    ItemVenta items[MAX_ITEMS_VENTA];
    int n_items = parsearCarrito(carrito, items, MAX_ITEMS_VENTA);
    if (n_items == 0) {
        printf("ERROR;Carrito_Vacio\n");
        return -1;
    }
    if (n_items < 0) {
        printf("ERROR;Carrito_Invalido\n");
        return -1;
    }

    const char *conninfo = getenv("FARMACIA_DB_CONN");
    if (conninfo == NULL || conninfo[0] == '\0') {
        printf("ERROR;FARMACIA_DB_CONN_no_definida\n");
        return -1;
    }

    PGconn *conn = PQconnectdb(conninfo);
    if (conn == NULL || PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "ventas: %s\n",
                conn != NULL ? PQerrorMessage(conn) : "sin conexion");
        printf("ERROR;conexion_bd_fallida\n");
        if (conn != NULL) {
            PQfinish(conn);
        }
        return -1;
    }

    PGresult *res = PQexec(conn, "BEGIN");
    if (res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "ventas BEGIN: %s\n", PQerrorMessage(conn));
        printf("ERROR;transaccion_fallida\n");
        if (res != NULL) {
            PQclear(res);
        }
        PQfinish(conn);
        return -1;
    }
    PQclear(res);

    /* Resolver el vendedor a partir del username. Si no se recibe username,
       la venta se registra con id_vendedor NULL (columna opcional). */
    char id_vendedor[16] = "";
    if (username != NULL && username[0] != '\0') {
        const char *sql_vendedor =
            "SELECT id FROM usuarios WHERE username = $1 AND activo = TRUE";
        const Oid tipos_vendedor[] = { 25 };
        const char *valores_vendedor[] = { username };
        res = PQexecParams(conn, sql_vendedor, 1, tipos_vendedor,
                           valores_vendedor, NULL, NULL, 0);
        if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
            fprintf(stderr, "ventas vendedor: %s\n", PQerrorMessage(conn));
            if (res != NULL) {
                PQclear(res);
            }
            rollbackYError(conn, "consulta_vendedor_fallida");
            PQfinish(conn);
            return -1;
        }
        if (PQntuples(res) == 0) {
            PQclear(res);
            rollbackYError(conn, "Vendedor_No_Encontrado");
            PQfinish(conn);
            return -1;
        }
        snprintf(id_vendedor, sizeof(id_vendedor), "%s", PQgetvalue(res, 0, 0));
        PQclear(res);
    }

    const char *dni_valor = (dni != NULL && dni[0] != '\0') ? dni : NULL;
    const char *sql_venta =
        "INSERT INTO venta (id_vendedor, dni_cliente, total) "
        "VALUES ($1, $2, 0) RETURNING id";
    const Oid tipos_venta[] = { 23, 25 };
    const char *valores_venta[] = {
        id_vendedor[0] != '\0' ? id_vendedor : NULL,
        dni_valor
    };
    res = PQexecParams(conn, sql_venta, 2, tipos_venta, valores_venta,
                       NULL, NULL, 0);
    if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "ventas INSERT venta: %s\n", PQerrorMessage(conn));
        if (res != NULL) {
            PQclear(res);
        }
        rollbackYError(conn, "registro_venta_fallido");
        PQfinish(conn);
        return -1;
    }
    char id_venta[16];
    snprintf(id_venta, sizeof(id_venta), "%s", PQgetvalue(res, 0, 0));
    PQclear(res);

    double total = 0.0;

    for (int i = 0; i < n_items; i++) {
        const char *sql_producto =
            "SELECT nombre, precio, stock FROM producto "
            "WHERE codigo_producto = $1 FOR UPDATE";
        const Oid tipos_producto[] = { 25 };
        const char *valores_producto[] = { items[i].codigo };
        res = PQexecParams(conn, sql_producto, 1, tipos_producto,
                           valores_producto, NULL, NULL, 0);

        if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
            fprintf(stderr, "ventas SELECT producto: %s\n", PQerrorMessage(conn));
            if (res != NULL) {
                PQclear(res);
            }
            rollbackYError(conn, "consulta_producto_fallida");
            PQfinish(conn);
            return -1;
        }

        if (PQntuples(res) == 0) {
            PQclear(res);
            char mensaje[96];
            snprintf(mensaje, sizeof(mensaje), "Producto_No_Encontrado:%.15s",
                     items[i].codigo);
            rollbackYError(conn, mensaje);
            PQfinish(conn);
            return -1;
        }

        double precio = strtod(PQgetvalue(res, 0, 1), NULL);
        int stock = atoi(PQgetvalue(res, 0, 2));
        PQclear(res);

        if (stock < items[i].cantidad) {
            char mensaje[96];
            snprintf(mensaje, sizeof(mensaje), "Stock_Insuficiente:%.15s",
                     items[i].codigo);
            rollbackYError(conn, mensaje);
            PQfinish(conn);
            return -1;
        }

        double subtotal = precio * items[i].cantidad;
        total += subtotal;

        char cantidad_txt[16];
        char subtotal_txt[32];
        snprintf(cantidad_txt, sizeof(cantidad_txt), "%d", items[i].cantidad);
        snprintf(subtotal_txt, sizeof(subtotal_txt), "%.2f", subtotal);

        const char *sql_detalle =
            "INSERT INTO detalle_venta "
            "(id_venta, codigo_producto, cantidad, subtotal) "
            "VALUES ($1, $2, $3, $4)";
        const Oid tipos_detalle[] = { 23, 25, 23, 1700 };
        const char *valores_detalle[] = {
            id_venta, items[i].codigo, cantidad_txt, subtotal_txt
        };
        res = PQexecParams(conn, sql_detalle, 4, tipos_detalle,
                           valores_detalle, NULL, NULL, 0);
        if (res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
            fprintf(stderr, "ventas INSERT detalle: %s\n", PQerrorMessage(conn));
            if (res != NULL) {
                PQclear(res);
            }
            rollbackYError(conn, "registro_detalle_fallido");
            PQfinish(conn);
            return -1;
        }
        PQclear(res);

        const char *sql_stock =
            "UPDATE producto SET stock = stock - $2 "
            "WHERE codigo_producto = $1";
        const Oid tipos_stock[] = { 25, 23 };
        const char *valores_stock[] = { items[i].codigo, cantidad_txt };
        res = PQexecParams(conn, sql_stock, 2, tipos_stock,
                           valores_stock, NULL, NULL, 0);
        if (res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
            fprintf(stderr, "ventas UPDATE stock: %s\n", PQerrorMessage(conn));
            if (res != NULL) {
                PQclear(res);
            }
            rollbackYError(conn, "actualizacion_stock_fallida");
            PQfinish(conn);
            return -1;
        }
        PQclear(res);
    }

    char total_txt[32];
    snprintf(total_txt, sizeof(total_txt), "%.2f", total);

    const char *sql_total = "UPDATE venta SET total = $2 WHERE id = $1";
    const Oid tipos_total[] = { 23, 1700 };
    const char *valores_total[] = { id_venta, total_txt };
    res = PQexecParams(conn, sql_total, 2, tipos_total,
                       valores_total, NULL, NULL, 0);
    if (res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "ventas UPDATE total: %s\n", PQerrorMessage(conn));
        if (res != NULL) {
            PQclear(res);
        }
        rollbackYError(conn, "actualizacion_total_fallida");
        PQfinish(conn);
        return -1;
    }
    PQclear(res);

    res = PQexec(conn, "COMMIT");
    if (res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "ventas COMMIT: %s\n", PQerrorMessage(conn));
        if (res != NULL) {
            PQclear(res);
        }
        rollbackYError(conn, "commit_fallido");
        PQfinish(conn);
        return -1;
    }
    PQclear(res);

    printf("OK;VENTA;%s;%.2f\n", id_venta, total);
    PQfinish(conn);
    return 0;
}
