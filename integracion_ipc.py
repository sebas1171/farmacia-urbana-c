"""Integración IPC: frontend Python <-> backend C (binario ./farmacia).

Contrato de salida del binario (stdout, campos separados por ';'):
    OK;<id>;<nombre>;<stock>
    ERROR;<motivo>

Ejemplo:
    ./farmacia buscar paracetamol
        -> OK;MED-001;Paracetamol;50
        -> ERROR;Stock_Insuficiente

La clase ConexionC envuelve el llamado con subprocess y normaliza
siempre la respuesta a un diccionario Python:
    {"status": "OK", "data": ["MED-001", "Paracetamol", "50"]}
    {"status": "ERROR", "mensaje": "Stock_Insuficiente"}
    {"status": "FATAL", "mensaje": "<detalle del error>"}
"""

from __future__ import annotations

import subprocess
from pathlib import Path


class ConexionC:
    """Puente IPC hacia el binario C `./farmacia` usando subprocess."""

    BASE_DIR = Path(__file__).resolve().parent
    NOMBRE_BINARIO = "farmacia"
    TIMEOUT_SEG = 5

    @classmethod
    def _ruta_del_binario(cls) -> Path:
        """Devuelve la ruta absoluta al binario C (mismo directorio que este módulo)."""
        return cls.BASE_DIR / cls.NOMBRE_BINARIO

    @classmethod
    def ejecutar(cls, *argumentos: str) -> dict[str, object]:
        """Ejecuta `./farmacia <argumentos...>` con argumentos dinámicos.

        Args:
            *argumentos: argumentos para el binario.
                Ejemplo: `ejecutar("buscar", "paracetamol")` ->
                    `./farmacia buscar paracetamol`.

        Returns:
            dict[str, object]: siempre un diccionario; nunca propaga la excepción al caller.
                - Éxito:      {"status": "OK", "data": [["MED-001", "Paracetamol", "5.50"]]}
                - Error del backend: {"status": "ERROR", "mensaje": "Stock_Insuficiente"}
                - Fallo del binario / excepción (no existe, segfault, etc.):
                              {"status": "FATAL", "mensaje": "<detalle del error>"}
        """
        comando = [str(cls._ruta_del_binario()), *argumentos]

        try:
            resultado = subprocess.run(
                comando,
                capture_output=True,
                text=True,
                check=False,  # los errores de negocio llegan en stdout ("ERROR;..."), no solo en el exit code
                timeout=cls.TIMEOUT_SEG,
                cwd=str(cls.BASE_DIR),
            )
        except Exception as ex:  # noqa: BLE001
            # Cubre: binario no existe (FileNotFoundError), timeout, etc.
            return {"status": "FATAL", "mensaje": str(ex)}

        # returncode < 0 => el proceso murió por señal (ej. Segmentation Fault, SIGABRT).
        if resultado.returncode is not None and resultado.returncode < 0:
            detalle = (resultado.stderr or resultado.stdout or "").strip()
            return {
                "status": "FATAL",
                "mensaje": detalle or f"El binario C termino inesperadamente (signal {resultado.returncode}).",
            }

        stdout = (resultado.stdout or "").strip()
        if not stdout:
            return {"status": "FATAL", "mensaje": "El backend no devolvio respuesta."}

        filas: list[list[str]] = []
        error: str | None = None

        # Procesa stdout linea por linea: el binario puede emitir varios
        # registros separados por newline, cada uno con su estado inicial.
        for linea in stdout.splitlines():
            linea = linea.strip()
            if not linea:
                continue
            partes = [p.strip() for p in linea.split(";") if p.strip()]
            estado = partes[0].upper() if partes else ""
            if estado == "OK":
                # "OK;MED-001;Paracetamol;5.50" -> ["MED-001", "Paracetamol", "5.50"]
                filas.append(partes[1:])
            elif estado == "ERROR" and error is None:
                error = "; ".join(partes[1:]) or estado

        if error is not None:
            # "ERROR;Stock_Insuficiente" -> mensaje = "Stock_Insuficiente"
            return {"status": "ERROR", "mensaje": error}

        if filas:
            # Éxito: cada registro OK es su propia lista independiente.
            # Ej: "OK;a1;a2\nOK;a3;a4" -> {"status": "OK", "data": [["a1", "a2"], ["a3", "a4"]]}
            return {"status": "OK", "data": filas}

        return {"status": "FATAL", "mensaje": "El backend no devolvio respuesta."}
