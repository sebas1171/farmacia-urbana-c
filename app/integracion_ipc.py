"""Integración IPC: frontend Python <-> backend C (binario ./farmacia).

Contrato de salida del binario (stdout, campos separados por ';'):
    OK;<campo1>;<campo2>;...
    OK;<TAG>;<campo1>;<campo2>;...
    OK;SIN_RESULTADOS
    ERROR;<motivo>

La clase ConexionC envuelve el llamado con subprocess y normaliza siempre
la respuesta a un diccionario Python:
    {"status": "OK", "data": [["MED-001", "Medicamento", "5.50"]]}
    {"status": "OK", "data": []}                 # OK;SIN_RESULTADOS
    {"status": "ERROR", "mensaje": "Stock_Insuficiente"}
    {"status": "FATAL", "mensaje": "<detalle del error>"}

Los tags de sección (FILA, STOCK, VENCIMIENTO, ALERTAS, VENTA,
PRODUCTO_MAS_VENDIDO, INGRESOS_TOTALES, ...) se eliminan para que las filas
tengan siempre los mismos campos que espera la interfaz. Los campos vacíos
se conservan (por ejemplo, un DNI de cliente no informado).
"""

from __future__ import annotations

import subprocess
from pathlib import Path


class ConexionC:
    """Puente IPC hacia el binario C `./farmacia` usando subprocess."""

    BASE_DIR = Path(__file__).resolve().parent
    NOMBRE_BINARIO = "farmacia"
    TIMEOUT_SEG = 10

    # Etiquetas que el backend agrega al inicio de una fila y que la UI no
    # debe mostrar como si fueran una columna de datos.
    TAGS = {
        "FILA",
        "STOCK",
        "VENCIMIENTO",
        "ALERTAS",
        "PRODUCTO_MAS_VENDIDO",
        "INGRESOS_TOTALES",
        "VENTA",
        "USUARIO",
        "USUARIO_CREADO",
        "USUARIO_ACTUALIZADO",
        "USUARIO_ELIMINADO",
    }

    @classmethod
    def _ruta_del_binario(cls) -> Path:
        """Devuelve la ruta absoluta al binario C (mismo directorio que este módulo)."""
        return cls.BASE_DIR / cls.NOMBRE_BINARIO

    @classmethod
    def _parsear(cls, texto: str) -> tuple[list[list[str]], str | None, bool]:
        """Convierte stdout/stderr en (filas, error, hubo_sin_resultados).

        - Conserva los campos vacíos: "OK;a;;c" -> ["a", "", "c"].
        - "OK;SIN_RESULTADOS" no genera una fila de datos.
        - Elimina el tag de sección de cada fila.
        """
        filas: list[list[str]] = []
        error: str | None = None
        sin_resultados = False

        for linea in texto.splitlines():
            linea = linea.strip()
            if not linea:
                continue

            partes = [p.strip() for p in linea.split(";")]
            estado = partes[0].upper() if partes else ""

            if estado == "OK":
                resto = partes[1:]
                if len(resto) == 1 and resto[0].upper() == "SIN_RESULTADOS":
                    sin_resultados = True
                    continue
                if resto and resto[0].upper() in cls.TAGS:
                    resto = resto[1:]
                if resto:
                    filas.append(resto)
            elif estado == "ERROR" and error is None:
                mensaje = "; ".join(partes[1:]).strip()
                error = mensaje or "ERROR"

        return filas, error, sin_resultados

    @classmethod
    def ejecutar(cls, *argumentos: str) -> dict[str, object]:
        """Ejecuta `./farmacia <argumentos...>` y normaliza la respuesta.

        Returns:
            dict[str, object]: siempre un diccionario; nunca propaga la excepción.
                - Éxito:      {"status": "OK", "data": [["MED-001", "Medicamento", "5.50"]]}
                - Sin datos:  {"status": "OK", "data": []}
                - Error del backend: {"status": "ERROR", "mensaje": "Stock_Insuficiente"}
                - Fallo del binario / excepción:
                              {"status": "FATAL", "mensaje": "<detalle del error>"}
        """
        comando = [str(cls._ruta_del_binario()), *argumentos]

        try:
            resultado = subprocess.run(
                comando,
                capture_output=True,
                text=True,
                check=False,
                timeout=cls.TIMEOUT_SEG,
                cwd=str(cls.BASE_DIR),
            )
        except Exception as ex:  # noqa: BLE001
            return {"status": "FATAL", "mensaje": str(ex)}

        # returncode < 0 => el proceso murió por señal (segfault, SIGABRT, etc.).
        if resultado.returncode is not None and resultado.returncode < 0:
            detalle = (resultado.stderr or resultado.stdout or "").strip()
            return {
                "status": "FATAL",
                "mensaje": detalle
                or f"El binario C termino inesperadamente (signal {resultado.returncode}).",
            }

        stdout = (resultado.stdout or "").strip()
        stderr = (resultado.stderr or "").strip()

        filas, error, sin_resultados = cls._parsear(stdout)

        # Si el backend no escribió nada en stdout, se intenta leer stderr.
        if not filas and error is None and not sin_resultados and stderr:
            filas, error, sin_resultados = cls._parsear(stderr)

        if error is not None:
            return {"status": "ERROR", "mensaje": error}

        if filas or sin_resultados:
            return {"status": "OK", "data": filas}

        detalle = stderr or stdout or "El backend no devolvio respuesta."
        return {"status": "FATAL", "mensaje": detalle}
