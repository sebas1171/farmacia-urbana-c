-- ============================================================
-- Base de datos del módulo de inventario
-- ============================================================

BEGIN;

CREATE TABLE IF NOT EXISTS categorias (
    id SERIAL PRIMARY KEY,
    nombre VARCHAR(50) UNIQUE NOT NULL
);

CREATE TABLE IF NOT EXISTS productos (
    codigo VARCHAR(20) PRIMARY KEY,
    nombre VARCHAR(100) NOT NULL,
    descripcion TEXT,
    id_categoria INT NOT NULL REFERENCES categorias(id),
    precio DECIMAL(10,2) NOT NULL CHECK (precio >= 0),
    stock INT NOT NULL DEFAULT 0 CHECK (stock >= 0),
    lote VARCHAR(50) NOT NULL,
    fecha_vencimiento DATE NOT NULL
);

-- Categorías necesarias para registrar los productos

INSERT INTO categorias (nombre)
VALUES
    ('Analgesicos'),
    ('Antibioticos'),
    ('Vitaminas')
ON CONFLICT (nombre) DO NOTHING;

-- Productos iniciales de prueba

INSERT INTO productos (
    codigo,
    nombre,
    descripcion,
    id_categoria,
    precio,
    stock,
    lote,
    fecha_vencimiento
)
VALUES
    (
        'MED-ANA-001',
        'Paracetamol',
        'Analgesico de uso general',
        (SELECT id FROM categorias WHERE nombre = 'Analgesicos'),
        5.50,
        20,
        'LOTE-PAR-001',
        DATE '2027-12-30'
    ),
    (
        'MED-ANA-002',
        'Ibuprofeno',
        'Analgesico y antiinflamatorio',
        (SELECT id FROM categorias WHERE nombre = 'Analgesicos'),
        8.90,
        15,
        'LOTE-IBU-001',
        DATE '2028-03-15'
    ),
    (
        'MED-ANT-001',
        'Amoxicilina',
        'Antibiotico para datos iniciales de prueba',
        (SELECT id FROM categorias WHERE nombre = 'Antibioticos'),
        18.50,
        10,
        'LOTE-AMO-001',
        DATE '2027-10-20'
    ),
    (
        'MED-VIT-001',
        'Vitamina C',
        'Suplemento vitaminico',
        (SELECT id FROM categorias WHERE nombre = 'Vitaminas'),
        12.00,
        30,
        'LOTE-VIT-001',
        DATE '2028-06-25'
    )
ON CONFLICT (codigo) DO NOTHING;

COMMIT;
