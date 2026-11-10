cat << 'EOF' > ventas.h
#ifndef VENTAS_H
#define VENTAS_H

// Buscar producto
void buscar_producto_venta(const char* codigo);

// Procesar venta (AHORA INCLUYE LOS DATOS PARA LOS REPORTES DE AZUMI)
void procesar_venta(const char* codigo, int cantidad, int id_vendedor, const char* dni_cliente);

#endif
EOF
