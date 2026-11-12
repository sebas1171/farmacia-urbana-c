#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>

/**
 * Procesa la pregunta del chatbot usando strstr y se conecta a PostgreSQL si es necesario.
 */
void procesar_chatbot(const char *pregunta) {
    // Convertir la pregunta a minúsculas localmente para asegurar una búsqueda robusta con strstr
    char preg_lower[512];
    strncpy(preg_lower, pregunta, sizeof(preg_lower) - 1);
    preg_lower[sizeof(preg_lower) - 1] = '\0';
    
    // Pasar a minúsculas de forma simple
    for(int i = 0; preg_lower[i]; i++){
        if(preg_lower[i] >= 'A' && preg_lower[i] <= 'Z') {
            preg_lower[i] += 32;
        }
    }

    // Caso A: Ayuda
    if (strstr(preg_lower, "como") != NULL || 
        strstr(preg_lower, "funciona") != NULL || 
        strstr(preg_lower, "cobrar") != NULL) {
        printf("CHAT;Para cobrar ve al modulo de ventas.\n");
        return;
    }

    // Caso B: Consulta BD (stock + nombre de medicamento)
    if (strstr(preg_lower, "stock") != NULL) {
        // Extraer el nombre del medicamento asumiendo el formato "stock de [medicamento]" o similar
        // Buscamos la palabra "stock" y tomamos lo que sigue
        char *ptr = strstr(preg_lower, "stock");
        char medicamento[100] = "";
        
        // Un parser sencillo para extraer el término posterior a "stock"
        // Ejemplo: "stock ibuprofeno" -> extrae "ibuprofeno"
        if (ptr != NULL) {
            // Avanzar después de la palabra "stock"
            ptr += 5;
            while (*ptr == ' ' || *ptr == 'd' || *ptr == 'e') ptr++; // limpiar espacios o "de"
            sscanf(ptr, "%s", medicamento);
        }

        if (strlen(medicamento) == 0) {
            printf("CHAT;Por favor especifica el nombre del medicamento a buscar.\n");
            return;
        }

        // Conexión a PostgreSQL para buscar el stock
        char conninfo[256];
        snprintf(conninfo, sizeof(conninfo), "dbname=farmacia user=postgres password=tu_password host=localhost port=5432");
        PGconn *conn = PQconnectdb(conninfo);

        if (PQstatus(conn) != CONNECTION_OK) {
            printf("CHAT;Error de conexion con la base de datos.\n");
            PQfinish(conn);
            return;
        }

        const char *query = "SELECT stock FROM inventario WHERE LOWER(nombre) = LOWER($1)";
        const char *paramValues[1] = { medicamento };

        PGresult *res = PQexecParams(conn, query, 1, NULL, paramValues, NULL, NULL, 0);

        if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
            char *stock_val = PQgetvalue(res, 0, 0);
            printf("CHAT;Actualmente tenemos %s unidades registradas.\n", stock_val);
        } else {
            printf("CHAT;No se encontro el medicamento '%s' en el inventario.\n", medicamento);
        }

        PQclear(res);
        PQfinish(conn);
        return;
    }

    // Si no coincide con ninguna regla anterior
    printf("CHAT;No he entendido tu consulta. Escribe 'ayuda' o consulta el stock.\n");
}