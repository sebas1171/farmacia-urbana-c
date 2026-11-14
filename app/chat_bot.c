#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libpq-fe.h>

#include "chat_bot.h"

/* -------------------------------------------------------------------------
 * Utilidades de texto
 * ---------------------------------------------------------------------- */

/* Elimina espacios al inicio y al final (in-place). */
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

/* Copia `s` recortando espacios y signos de puntuación de los extremos
   (in-place, moviendo el contenido). */
static void recortar(char *s) {
    char *limpio = trim(s);
    if (limpio != s) {
        memmove(s, limpio, strlen(limpio) + 1);
    }
    size_t len = strlen(s);
    while (len > 0 && strchr(", .?!:;", s[len - 1]) != NULL) {
        s[--len] = '\0';
    }
    limpio = trim(s);
    if (limpio != s) {
        memmove(s, limpio, strlen(limpio) + 1);
    }
}

/* Normaliza el texto a minúsculas ASCII sin acentos.
   Así "¿Cómo hago una VENTA?" y "como hago una venta" producen la misma
   cadena y las claves de la base de conocimientos se comparan siempre. */
static void normalizar_texto(const char *origen, char *destino, size_t tam) {
    size_t j = 0;
    for (size_t i = 0; origen[i] != '\0' && j + 1 < tam;) {
        unsigned char c = (unsigned char)origen[i];

        if (c < 0x80) {
            destino[j++] = (char)tolower(c);
            i++;
            continue;
        }

        /* Vocales acentuadas y eñe en UTF-8 (2 bytes): se convierten a ASCII. */
        if (c == 0xC3 && (unsigned char)origen[i + 1] != '\0') {
            unsigned char s = (unsigned char)origen[i + 1];
            char base = '\0';
            switch (s) {
                case 0xA1: case 0xA0: case 0xA2: case 0xA3: case 0xA4:
                case 0x81: case 0x80: case 0x82: case 0x83: case 0x84:
                    base = 'a'; break;
                case 0xA9: case 0xA8: case 0xAA: case 0xAB:
                case 0x89: case 0x88: case 0x8A: case 0x8B:
                    base = 'e'; break;
                case 0xAD: case 0xAC: case 0xAE: case 0xAF:
                case 0x8D: case 0x8C: case 0x8E: case 0x8F:
                    base = 'i'; break;
                case 0xB3: case 0xB2: case 0xB4: case 0xB5: case 0xB6:
                case 0x93: case 0x92: case 0x94: case 0x95: case 0x96:
                    base = 'o'; break;
                case 0xBA: case 0xB9: case 0xBB: case 0xBC:
                case 0x9A: case 0x99: case 0x9B: case 0x9C:
                    base = 'u'; break;
                case 0xB1: case 0x91:
                    base = 'n'; break;
                default:
                    break;
            }
            if (base != '\0') {
                destino[j++] = base;
                i += 2;
                continue;
            }
            i += 2; /* otros caracteres de 2 bytes (p. ej. ¿ ¡) se omiten */
            continue;
        }

        /* Caracteres multibyte restantes: se omiten completos. */
        size_t paso = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : 2;
        size_t k = 1;
        while (k < paso && origen[i + k] != '\0') {
            k++;
        }
        i += k;
    }
    destino[j] = '\0';
}

/* Busca una palabra o frase respetando límites de palabra.
   Evita falsos positivos como "admin" dentro de "administrar" o "cmp"
   dentro de otra palabra. */
static int contiene_clave(const char *texto, const char *clave) {
    size_t len = strlen(clave);
    if (len == 0) {
        return 0;
    }
    const char *p = texto;
    while ((p = strstr(p, clave)) != NULL) {
        int inicio_ok = (p == texto) || !isalnum((unsigned char)p[-1]);
        int fin_ok = !isalnum((unsigned char)p[len]);
        if (inicio_ok && fin_ok) {
            return 1;
        }
        p += len;
    }
    return 0;
}

/* Palabras de relleno que suelen aparecer entre la consulta y el producto. */
static int es_relleno(const char *p) {
    static const char *palabras[] = {
        "de", "del", "el", "la", "los", "las", "un", "una",
        "hay", "en", "tiene", "tienes", "queda", "quedan",
        "disponible", "disponibles", "unidad", "unidades",
        "cuanto", "cuanta", "cuantos", "cuantas", "es", "esta",
        "stock", "por", "favor", "me", "puedes", "decir",
        "cuesta", "cuestan", "vale", "valen", "dime", "saber",
        "consulto", "consultar", "ver", "donde", "cual", "cuales",
        "como", "precio", "precios", "medicamento", "medicamentos"
    };
    for (size_t i = 0; i < sizeof(palabras) / sizeof(palabras[0]); i++) {
        size_t len = strlen(palabras[i]);
        if (strncmp(p, palabras[i], len) == 0 &&
            !isalnum((unsigned char)p[len])) {
            return (int)len;
        }
    }
    return 0;
}

/* Salta espacios y palabras de relleno hasta llegar al nombre del producto. */
static const char *saltar_relleno(const char *p) {
    for (;;) {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        int len = es_relleno(p);
        if (len == 0) {
            break;
        }
        p += len;
    }
    return p;
}

/* Quita del final las palabras de relleno ("amoxicilina hay" -> "amoxicilina"). */
static void quitar_relleno_final(char *s) {
    for (;;) {
        size_t len = strlen(s);
        if (len == 0) {
            return;
        }
        char *ultima = s;
        for (char *q = s; *q != '\0'; q++) {
            if (isspace((unsigned char)*q)) {
                ultima = q + 1;
            }
        }
        size_t len_ultima = strlen(ultima);
        if (len_ultima == 0) {
            return;
        }
        int relleno = es_relleno(ultima);
        if (relleno == (int)len_ultima) {
            *ultima = '\0';
            recortar(s);
        } else {
            return;
        }
    }
}

/* Extrae el término de búsqueda que sigue a una palabra clave. */
static void extraer_termino(const char *pregunta_lower, const char *clave,
                            char *destino, size_t tam) {
    destino[0] = '\0';
    const char *p = strstr(pregunta_lower, clave);
    if (p == NULL) {
        return;
    }

    /* 1) Texto que aparece después de la clave: "stock de paracetamol". */
    const char *resto = saltar_relleno(p + strlen(clave));
    size_t i = 0;
    while (*resto != '\0' && *resto != '?' && *resto != '.' && *resto != ',' &&
           *resto != '!' && *resto != ';' && i + 1 < tam) {
        destino[i++] = *resto++;
    }
    destino[i] = '\0';
    recortar(destino);
    if (destino[0] != '\0') {
        return;
    }

    /* 2) Si no hay texto después, se intenta con el texto anterior:
          "paracetamol stock". */
    size_t n = (size_t)(p - pregunta_lower);
    if (n >= tam) {
        n = tam - 1;
    }
    memcpy(destino, pregunta_lower, n);
    destino[n] = '\0';
    const char *ini = saltar_relleno(destino);
    if (ini != destino) {
        memmove(destino, ini, strlen(ini) + 1);
    }
    recortar(destino);
}

/* Imprime una respuesta en una sola línea del contrato IPC.
   ';' separa campos y '\n' separa registros. Para permitir textos de varias
   líneas en la interfaz, cada salto de línea se codifica como la secuencia
   literal "\n" (barra invertida + n) y el frontend la reconstruye. */
static void imprimir_ok(const char *mensaje) {
    char limpio[2048];
    size_t j = 0;

    for (size_t i = 0; mensaje[i] != '\0'; i++) {
        char c = mensaje[i];
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            if (j + 2 >= sizeof(limpio)) {
                break;
            }
            limpio[j++] = '\\';
            limpio[j++] = 'n';
            continue;
        }
        if (j + 1 >= sizeof(limpio)) {
            break;
        }
        limpio[j++] = (c == ';') ? ' ' : c;
    }
    limpio[j] = '\0';
    printf("OK;%s\n", limpio);
}

/* -------------------------------------------------------------------------
 * Base de conocimientos estática (FAQ del asistente virtual)
 * ---------------------------------------------------------------------- */

#define MAX_CLAVES_FAQ 12

typedef struct {
    const char *claves[MAX_CLAVES_FAQ];
    const char *respuesta;
} EntradaFAQ;

static const EntradaFAQ BASE_CONOCIMIENTOS[] = {
    {
        { "quien eres", "como te llamas", "cual es tu nombre", "tu nombre",
          "que eres", "quien es urbano", NULL },
        "Soy Urbano, el asistente virtual de Farmacia Urbana. Estoy aqui para "
        "consultar el stock y el vencimiento de tus medicamentos y para "
        "guiarte en el uso del sistema. Preguntame por ventas, CMP, reportes, "
        "inventario, alertas, usuarios o sucursales."
    },
    {
        { "hago una venta", "hacer una venta", "registrar una venta",
          "registrar venta", "nueva venta", "como vender", "vender un medicamento",
          "procesar venta", "procesar una venta", "venta", "vender", NULL },
        "Para registrar una venta:\n"
        "1) Abre el modulo Ventas.\n"
        "2) Busca el medicamento por nombre o codigo y agrega la cantidad al carrito.\n"
        "3) Si el producto requiere receta, ingresa el CMP del medico cuando se solicite.\n"
        "4) Escribe el DNI del cliente (8 digitos) o dejalo vacio.\n"
        "5) Pulsa Procesar Venta.\n"
        "El sistema valida el stock, descuenta las unidades y genera la boleta. "
        "Si no hay stock suficiente, la venta se rechaza y veras el motivo."
    },
    {
        { "cmp", "colegiatura", "colegiatura medica", "receta", "recetas",
          "numero de colegiatura", NULL },
        "El CMP es el numero de Colegiatura Medica del medico que prescribe. "
        "Se exige al vender medicamentos que requieren receta: el vendedor lo "
        "registra durante la venta y queda asociado a la boleta para auditoria. "
        "El rol ADMIN esta exento de ingresarlo y, si el producto no requiere "
        "receta, el campo puede quedar vacio."
    },
    {
        { "administrador", "quien administra", "rol admin", "roles", "permisos",
          "que puede hacer el admin", "admin", NULL },
        "El administrador es el usuario con rol ADMIN. Es el unico que puede "
        "gestionar usuarios, crear, editar o eliminar productos del inventario "
        "y consultar todos los reportes. Los roles VENDEDOR y TECNICO tienen "
        "permisos limitados. Si necesitas una tarea administrativa, solicitala "
        "al ADMIN de tu sucursal."
    },
    {
        { "veo los reportes", "ver reportes", "como veo los reportes",
          "reportes", "reporte", "historial", "historial de ventas", "ingresos",
          "producto mas vendido", "estadisticas", NULL },
        "Para ver los reportes:\n"
        "1) Inicia sesion con un usuario ADMIN.\n"
        "2) Abre el modulo Reportes.\n"
        "3) Elige la fecha de inicio y la fecha fin (formato YYYY-MM-DD).\n"
        "4) Pulsa Generar Historial.\n"
        "Veras el historial de ventas, los ingresos totales y el producto mas "
        "vendido. En el modulo Alertas encontraras el stock bajo y los proximos "
        "vencimientos, disponible para todos los roles."
    },
    {
        { "ayuda", "ayudame", "que puedes hacer", "que sabes hacer", "opciones",
          "comandos", "instrucciones", "como funciona", "menu", "soporte", NULL },
        "¡Claro! Soy Urbano y puedo ayudarte con:\n"
        "1) Consultar stock y vencimiento: escribe por ejemplo 'stock de paracetamol'.\n"
        "2) Explicarte como registrar una venta.\n"
        "3) Contarte que es el CMP y cuando se exige.\n"
        "4) Guiarte para ver reportes, inventario, alertas y usuarios.\n"
        "5) Decirte quien administra el sistema y como cambiar de sucursal.\n"
        "Escribe tu duda con palabras simples y yo te oriento."
    },
    {
        { "inventario", "crear producto", "agregar producto", "nuevo producto",
          "editar producto", "eliminar producto", "buscar producto",
          "codigo de producto", "categoria", NULL },
        "El modulo Inventario te permite buscar productos y, si eres ADMIN, "
        "crear, editar o eliminar medicamentos. Cada producto guarda codigo, "
        "nombre, precio, stock, lote, fecha de vencimiento y categoria. Usa el "
        "buscador por nombre o codigo para localizarlo rapido. Para consultar "
        "solo el stock de un medicamento, preguntame por ejemplo "
        "'stock de ibuprofeno'."
    },
    {
        { "alertas", "alerta", "avisos", "notificaciones", NULL },
        "El modulo Alertas muestra dos tipos de aviso: productos con stock por "
        "debajo del minimo y productos proximos a vencer o ya vencidos. "
        "Revisalo al iniciar el turno para reponer mercaderia o retirar "
        "productos vencidos. Esta disponible para todos los roles."
    },
    {
        { "usuarios", "usuario", "crear usuario", "gestion de usuarios",
          "cuentas", "contrasena", "password", NULL },
        "La gestion de usuarios esta reservada al rol ADMIN. En el modulo "
        "Usuarios puedes crear cuentas con nombre, usuario, contrasena, rol "
        "(ADMIN, VENDEDOR o TECNICO) y sucursal. Tambien puedes cambiar el rol "
        "o eliminar una cuenta. Cada usuario opera solo sobre la sucursal de "
        "su sesion."
    },
    {
        { "sucursal", "sucursales", "cambiar de sucursal", "mi local",
          "cerrar sesion", "sesion", "multi-local", NULL },
        "Tu sesion esta asociada a una sucursal: las consultas de stock, las "
        "ventas y las alertas se filtran por ese local. El ADMIN puede cambiar "
        "de sucursal desde el selector de la esquina superior derecha. Para "
        "cerrar sesion usa el boton rojo del menu lateral."
    },
    {
        { "gracias", "muchas gracias", "te lo agradezco", "agradecido", NULL },
        "¡Con gusto! Si necesitas algo mas, aqui estoy. Recuerda que puedo "
        "consultar el stock y el vencimiento de cualquier medicamento de tu "
        "sucursal."
    },
    {
        { "adios", "hasta luego", "nos vemos", "chao", "bye", "hasta pronto", NULL },
        "¡Hasta pronto! Soy Urbano, tu asistente virtual de Farmacia Urbana. "
        "Cuando necesites consultar stock o resolver una duda del sistema, "
        "aqui estare."
    }
};

/* Devuelve la respuesta FAQ que coincide con la pregunta, o NULL. */
static const char *buscar_faq(const char *pregunta_lower) {
    size_t total = sizeof(BASE_CONOCIMIENTOS) / sizeof(BASE_CONOCIMIENTOS[0]);
    for (size_t i = 0; i < total; i++) {
        for (size_t j = 0; j < MAX_CLAVES_FAQ; j++) {
            const char *clave = BASE_CONOCIMIENTOS[i].claves[j];
            if (clave == NULL) {
                break;
            }
            if (contiene_clave(pregunta_lower, clave)) {
                return BASE_CONOCIMIENTOS[i].respuesta;
            }
        }
    }
    return NULL;
}

/* Detecta un saludo en cualquier parte del mensaje. */
static int es_saludo(const char *pregunta_lower) {
    static const char *claves[] = {
        "hola", "buenas", "buenos", "buen dia", "saludos", "hey", "holi",
        "que tal"
    };
    for (size_t i = 0; i < sizeof(claves) / sizeof(claves[0]); i++) {
        if (contiene_clave(pregunta_lower, claves[i])) {
            return 1;
        }
    }
    return 0;
}

/* Palabras que indican una consulta real de inventario.
   Cuando aparecen, la pregunta se resuelve contra PostgreSQL. */
static int es_consulta_inventario(const char *pregunta_lower) {
    static const char *claves[] = {
        "stock", "cantidad", "cantidades", "existencia", "existencias",
        "disponible", "disponibles", "unidad", "unidades",
        "caducidad", "caducado", "caducada", "caduca",
        "vencimiento", "vencido", "vencida", "vencidos", "vencidas",
        "vence", "vencen"
    };
    for (size_t i = 0; i < sizeof(claves) / sizeof(claves[0]); i++) {
        if (contiene_clave(pregunta_lower, claves[i])) {
            return 1;
        }
    }
    return 0;
}

static const char *claves_termino[] = {
    "stock", "existencias", "existencia", "disponibles", "disponible",
    "cantidades", "cantidad", "unidades", "unidad",
    "caducidad", "caducado", "caducada", "caduca",
    "vencimiento", "vencidos", "vencidas", "vencido", "vencida",
    "vencen", "vence",
    "tienes", "tiene", "hay", "cuantos", "cuantas", "cuanto", "cuanta"
};

/* -------------------------------------------------------------------------
 * Consulta de inventario en PostgreSQL
 * ---------------------------------------------------------------------- */

/* Imprime una respuesta; si el usuario saludó, antepone la presentación
   de Urbano para mantener la conversación natural. */
static void imprimir_asistente(const char *mensaje, int presentarse) {
    if (!presentarse) {
        imprimir_ok(mensaje);
        return;
    }
    char texto[1800];
    snprintf(texto, sizeof(texto),
             "¡Hola! Soy Urbano, el asistente virtual de Farmacia Urbana.\n%s",
             mensaje);
    imprimir_ok(texto);
}

static void consultar_inventario(const char *pregunta_lower, int id_sucursal,
                                 int presentarse) {
    char termino[100] = "";
    int clave_encontrada = 0;
    size_t total_claves = sizeof(claves_termino) / sizeof(claves_termino[0]);
    for (size_t i = 0; i < total_claves; i++) {
        if (strstr(pregunta_lower, claves_termino[i]) != NULL) {
            clave_encontrada = 1;
            extraer_termino(pregunta_lower, claves_termino[i],
                            termino, sizeof(termino));
            quitar_relleno_final(termino);
            break;
        }
    }

    if (termino[0] == '\0' && !clave_encontrada) {
        /* Sin palabra clave: se trata toda la pregunta como nombre/código. */
        const char *p = saltar_relleno(pregunta_lower);
        size_t i = 0;
        while (*p != '\0' && *p != '?' && *p != '.' && *p != ',' &&
               *p != '!' && *p != ';' && i + 1 < sizeof(termino)) {
            termino[i++] = *p++;
        }
        termino[i] = '\0';
        recortar(termino);
        quitar_relleno_final(termino);
    }

    if (termino[0] == '\0') {
        imprimir_asistente(
            "Dime el nombre o el codigo del medicamento que quieres "
            "consultar. Ejemplo: stock de paracetamol.",
            presentarse);
        return;
    }

    const char *conninfo = getenv("FARMACIA_DB_CONN");
    if (conninfo == NULL || conninfo[0] == '\0') {
        printf("ERROR;FARMACIA_DB_CONN_no_definida\n");
        return;
    }

    PGconn *conn = PQconnectdb(conninfo);
    if (conn == NULL || PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "chatbot: %s\n",
                conn != NULL ? PQerrorMessage(conn) : "sin conexion");
        printf("ERROR;conexion_bd_fallida\n");
        if (conn != NULL) {
            PQfinish(conn);
        }
        return;
    }

    char sucursal_txt[16];
    snprintf(sucursal_txt, sizeof(sucursal_txt), "%d", id_sucursal);

    /* Consulta real sobre `producto` (la misma tabla que descuenta ventas).
       ILIKE permite buscar por nombre o código sin distinguir mayúsculas.
       $2 limita la respuesta al inventario de la sucursal de la sesión. */
    const char *sql =
        "SELECT nombre, stock, "
        "COALESCE(TO_CHAR(fecha_vencimiento, 'YYYY-MM-DD'), "
        "'sin vencimiento registrado') "
        "FROM producto "
        "WHERE (nombre ILIKE '%' || $1 || '%' "
        "   OR codigo_producto ILIKE '%' || $1 || '%') "
        "AND id_sucursal = $2 "
        "ORDER BY nombre ASC LIMIT 1";
    const Oid paramTypes[] = { 25, 23 };
    const char *const paramValues[] = { termino, sucursal_txt };
    PGresult *res = PQexecParams(conn, sql, 2, paramTypes, paramValues,
                                 NULL, NULL, 0);

    if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "chatbot: %s\n",
                res != NULL ? PQerrorMessage(conn) : "sin resultado");
        printf("ERROR;consulta_bd_fallida\n");
        if (res != NULL) {
            PQclear(res);
        }
        PQfinish(conn);
        return;
    }

    if (PQntuples(res) > 0) {
        char respuesta[1024];
        const char *nombre = PQgetvalue(res, 0, 0);
        const char *stock = PQgetvalue(res, 0, 1);
        const char *vence = PQgetvalue(res, 0, 2);
        if (strcmp(vence, "sin vencimiento registrado") == 0) {
            snprintf(respuesta, sizeof(respuesta),
                     "%s tiene %s unidades disponibles, "
                     "no tiene vencimiento registrado.",
                     nombre, stock);
        } else {
            snprintf(respuesta, sizeof(respuesta),
                     "%s tiene %s unidades disponibles y vence el %s.",
                     nombre, stock, vence);
        }
        imprimir_asistente(respuesta, presentarse);
    } else {
        char respuesta[512];
        snprintf(respuesta, sizeof(respuesta),
                 "No encontre '%s' en el inventario de tu sucursal. "
                 "Revisa el nombre o el codigo. Si tu consulta no era sobre un "
                 "medicamento, escribe 'ayuda' para ver lo que puedo hacer.",
                 termino);
        imprimir_asistente(respuesta, presentarse);
    }

    PQclear(res);
    PQfinish(conn);
}

/* -------------------------------------------------------------------------
 * Punto de entrada del chatbot
 * ---------------------------------------------------------------------- */

void procesar_chatbot(const char *pregunta, int id_sucursal) {
    if (pregunta == NULL || pregunta[0] == '\0') {
        printf("ERROR;Pregunta_Vacia\n");
        return;
    }
    if (id_sucursal <= 0) {
        printf("ERROR;Sucursal_Invalida\n");
        return;
    }

    char preg_lower[512];
    normalizar_texto(pregunta, preg_lower, sizeof(preg_lower));
    int saludo = es_saludo(preg_lower);

    /* Precedencia:
       1) Consultas de inventario -> PostgreSQL (stock, cantidad, vencimiento).
       2) Preguntas frecuentes del sistema -> base de conocimientos.
       3) Saludos -> presentación de Urbano.
       4) Cualquier otro texto -> se interpreta como nombre de medicamento. */
    if (es_consulta_inventario(preg_lower)) {
        consultar_inventario(preg_lower, id_sucursal, saludo);
        return;
    }

    const char *respuesta_faq = buscar_faq(preg_lower);
    if (respuesta_faq != NULL) {
        imprimir_asistente(respuesta_faq, saludo);
        return;
    }

    if (saludo) {
        imprimir_ok("¡Hola! Soy Urbano, el asistente virtual de Farmacia "
                    "Urbana. Puedo consultar el stock y el vencimiento de un "
                    "medicamento en tu sucursal y guiarte con ventas, CMP, "
                    "reportes, inventario y alertas. Prueba con "
                    "'stock de paracetamol' o escribe 'ayuda' para ver todo "
                    "lo que puedo hacer por ti.");
        return;
    }

    consultar_inventario(preg_lower, id_sucursal, 0);
}
