-- 1. Tabla Usuarios
CREATE TABLE usuarios (
    id_usuario SERIAL PRIMARY KEY,
    nombre VARCHAR(100) NOT NULL,
    rol VARCHAR(50) NOT NULL,
    password VARCHAR(100) NOT NULL
);

-- 2. Tablas de Inventario
CREATE TABLE categorias (
    id_categoria SERIAL PRIMARY KEY,
    nombre VARCHAR(100) NOT NULL
);

CREATE TABLE productos (
    codigo VARCHAR(50) PRIMARY KEY,
    nombre VARCHAR(100) NOT NULL,
    precio DECIMAL(10,2) NOT NULL,
    stock INT NOT NULL,
    id_categoria INT REFERENCES categorias(id_categoria)
);

-- 3. Tablas de Ventas
CREATE TABLE ventas (
    id_venta SERIAL PRIMARY KEY,
    id_vendedor INT REFERENCES usuarios(id_usuario),
    dni_cliente VARCHAR(15), 
    fecha_hora TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    total DECIMAL(10,2) NOT NULL
);

CREATE TABLE detalle_ventas (
    id_detalle SERIAL PRIMARY KEY,
    id_venta INT REFERENCES ventas(id_venta) ON DELETE CASCADE,
    codigo_producto VARCHAR(50) REFERENCES productos(codigo), 
    cantidad INT NOT NULL,
    precio_unitario DECIMAL(10,2) NOT NULL,
    subtotal DECIMAL(10,2) NOT NULL
);

-- 4. DATOS DE PRUEBA
INSERT INTO usuarios (nombre, rol, password) VALUES 
('Sebastian Herrera', 'Administrador', 'admin123'), 
('Paolo', 'Vendedor', 'venta123');

INSERT INTO categorias (nombre) VALUES 
('Analgésicos'), 
('Antibióticos');

INSERT INTO productos (codigo, nombre, precio, stock, id_categoria) VALUES 
('P001', 'Paracetamol 500mg', 2.50, 100, 1), 
('P002', 'Amoxicilina 500mg', 5.00, 50, 2);
