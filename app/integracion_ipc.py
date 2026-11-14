"""Integración IPC: frontend Python <-> backend C (binario ./farmacia).

Contrato de salida del binario (stdout, campos separados por ';'):
    LOGIN_OK;<rol>;<nombre>;<id_sucursal>;<nombre_sucursal>
    OK;<campo1>;<campo2>;...
    OK;<TAG>;<campo1>;<campo2>;...
    OK;SIN_RESULTADOS
    ERROR;<motivo>

Sesión multi-local: tras el login, la interfaz registra la sucursal con
ConexionC.establecer_sucursal(id) y el puente agrega automáticamente ese
id_sucursal como último argumento de todos los comandos operativos.

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

    # Sucursal de la sesión actual. La fija FarmaciaApp después del login y
    # se inyecta en cada petición operativa para aislar los datos por local.
    id_sucursal_actual: int | None = None

    # Comandos que exigen id_sucursal (se agrega como último argumento).
    COMANDOS_CON_SUCURSAL = {
        "buscar_prod",
        "buscar_prod_codigo",
        "crear_prod",
        "editar_prod",
        "eliminar_prod",
        "procesar_venta",
        "historial",
        "producto_mas_vendido",
        "ingresos_totales",
        "alertas",
        "alertas_stock",
        "alertas_vencimientos",
        "chatbot",
        "listar_usuarios",
        "crear_usuario",
        "editar_usuario",
        "eliminar_usuario",
    }

    # Etiquetas que el backend agrega al inicio de una fila y que la UI no
    # debe mostrar como si fueran una columna de datos.
    TAGS = {
        "FILA",
        "SUCURSAL",
        "STOCK",
        "VENCIMIENTO",
        "ALERTAS",
        "PRODUCTO_MAS_VENDIDO",
        "INGRESOS_TOTALES",
        "VENTA",
        "USUARIO",
        # Respuestas de éxito del CRUD que no aportan columnas a una tabla.
        "USUARIO_CREADO",
        "USUARIO_ACTUALIZADO",
        "USUARIO_ELIMINADO",
        "PRODUCTO_CREADO",
        "PRODUCTO_ACTUALIZADO",
        "PRODUCTO_ELIMINADO",
    }

    @classmethod
    def establecer_sucursal(cls, id_sucursal: int | str) -> None:
        """Fija la sucursal de la sesión activa (se envía en cada petición)."""
        cls.id_sucursal_actual = int(str(id_sucursal).strip())

    @classmethod
    def limpiar_sucursal(cls) -> None:
        """Olvida la sucursal al cerrar sesión."""
        cls.id_sucursal_actual = None

    @classmethod
    def _ruta_del_binario(cls) -> Path:
        """Devuelve la ruta absoluta al binario C (mismo directorio que este módulo)."""
        return cls.BASE_DIR / cls.NOMBRE_BINARIO

    @classmethod
    def _parsear(
        cls, texto: str
    ) -> tuple[list[list[str]], str | None, bool, bool]:
        """Convierte stdout/stderr en (filas, error, sin_resultados, hubo_ok).

        - Conserva los campos vacíos: "OK;a;;c" -> ["a", "", "c"].
        - "OK;SIN_RESULTADOS" no genera una fila de datos.
        - "OK;Usuario_Creado" / "OK;Usuario_Actualizado" son éxitos aunque
          no aporten columnas para una tabla (hubo_ok=True, data=[]).
        - Elimina el tag de sección de cada fila.
        """
        filas: list[list[str]] = []
        error: str | None = None
        sin_resultados = False
        hubo_ok = False

        for linea in texto.splitlines():
            linea = linea.strip()
            if not linea:
                continue

            partes = [p.strip() for p in linea.split(";")]
            estado = partes[0].upper() if partes else ""

            if estado in ("OK", "LOGIN_OK"):
                hubo_ok = True
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

        return filas, error, sin_resultados, hubo_ok

    @classmethod
    def ejecutar(
        cls, *argumentos: str, sucursal: int | str | None = None
    ) -> dict[str, object]:
        """Ejecuta `./farmacia <argumentos...>` y normaliza la respuesta.

        Si el comando está en COMANDOS_CON_SUCURSAL, se agrega id_sucursal como
        último argumento. Se usa `sucursal` si se indica (por ejemplo al crear
        o editar un usuario en otro local) o la sucursal de la sesión actual.

        Returns:
            dict[str, object]: siempre un diccionario; nunca propaga la excepción.
                - Éxito:      {"status": "OK", "data": [["MED-001", "Medicamento", "5.50"]]}
                - Sin datos:  {"status": "OK", "data": []}
                - Error del backend: {"status": "ERROR", "mensaje": "Stock_Insuficiente"}
                - Fallo del binario / excepción:
                              {"status": "FATAL", "mensaje": "<detalle del error>"}
        """
        argumentos_limpios = [str(arg) for arg in argumentos]
        nombre_comando = (
            argumentos_limpios[0].strip().lower() if argumentos_limpios else ""
        )

        if nombre_comando in cls.COMANDOS_CON_SUCURSAL:
            valor_sucursal = (
                sucursal if sucursal is not None else cls.id_sucursal_actual
            )
            if valor_sucursal is None:
                return {
                    "status": "ERROR",
                    "mensaje": "Sucursal_no_inicializada",
                }
            argumentos_limpios.append(str(valor_sucursal))

        comando = [str(cls._ruta_del_binario()), *argumentos_limpios]

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

        filas, error, sin_resultados, hubo_ok = cls._parsear(stdout)

        # Si el backend no escribió nada en stdout, se intenta leer stderr.
        if not filas and error is None and not sin_resultados and not hubo_ok and stderr:
            filas, error, sin_resultados, hubo_ok = cls._parsear(stderr)

        if error is not None:
            return {"status": "ERROR", "mensaje": error}

        # hubo_ok cubre respuestas como OK;Usuario_Creado (éxito sin tabla).
        if filas or sin_resultados or hubo_ok:
            return {"status": "OK", "data": filas}

        detalle = stderr or stdout or "El backend no devolvio respuesta."
        return {"status": "FATAL", "mensaje": detalle}
