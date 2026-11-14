#ifndef CHAT_BOT_H
#define CHAT_BOT_H

/* Asistente virtual del sistema (Urbano).
   - Responde preguntas frecuentes (ventas, CMP, administrador, reportes,
     inventario, alertas, usuarios, sucursales y ayuda) con una base de
     conocimientos estática definida en chat_bot.c.
   - Se presenta al recibir un saludo.
   - Consulta stock y vencimiento reales en PostgreSQL (tabla producto)
     cuando la pregunta menciona stock, cantidad, vencimiento o el nombre de
     un medicamento. Lee la cadena de conexión de FARMACIA_DB_CONN y filtra
     por la sucursal indicada en id_sucursal.

   Contrato de salida (stdout):
     OK;<mensaje>
     ERROR;<motivo>
   Los saltos de línea internos del mensaje viajan como la secuencia "\n". */
void procesar_chatbot(const char *pregunta, int id_sucursal);

#endif
