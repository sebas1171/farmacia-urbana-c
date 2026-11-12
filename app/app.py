"""Sistema de Farmacia Urbana — frontend CustomTkinter + backend C.

Arquitectura monofichero (fácil de dividir después):
    FarmaciaApp (controlador raíz / CTk)
      |- LoginFrame        -> pantalla inicial de autenticación
      |- DashboardFrame    -> layout sidebar + área central dinámica
           |- InventarioView / VentasView / ReportesView (placeholders expandibles)

Backend C (ver main.c):
    ./farmacia login <usuario> <password>  -> stdout "OK;login_correcto" | "ERROR;..."
"""

from __future__ import annotations

import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

import customtkinter as ctk

# ---------------------------------------------------------------------------
# Configuración global
# ---------------------------------------------------------------------------
BASE_DIR = Path(__file__).resolve().parent
FARMACIA_BIN = BASE_DIR / "farmacia"  # binario C compilado junto a app.py
LOGIN_TIMEOUT_SEG = 5

ctk.set_appearance_mode("System")       # "System" | "Dark" | "Light"
ctk.set_default_color_theme("blue")     # "blue" | "green" | "dark-blue"


# ---------------------------------------------------------------------------
# Modelo + BackendService: único punto de contacto con el binario C.
# Si en el futuro se divide el proyecto, mover estas clases a
# models/producto.py y services/backend.py sin cambiar sus firmas.
# ---------------------------------------------------------------------------
@dataclass
class Producto:
    """Producto del inventario. Refleja `struct Producto` de archivos.h."""

    id: int
    nombre: str
    precio: float
    stock: int
    lote: str
    vencimiento: str

    @property
    def stock_bajo(self) -> bool:
        return self.stock < 10


@dataclass
class ReporteResumen:
    """Estadísticas globales para ReportesView.

    Refleja el contrato de `./farmacia reporte`:
        OK;total=N;alertas=M;valorizado=X;stock_total=Y
        ALERTA;id;nombre;stock
        ...
    """

    total_productos: int = 0
    alertas: int = 0
    valorizado: float = 0.0
    stock_total: int = 0
    detalle_alertas: list[tuple[int, str, int]] | None = None

    def __post_init__(self) -> None:
        if self.detalle_alertas is None:
            self.detalle_alertas = []


class BackendService:
    """Puente Python -> C vía subprocess."""

    @staticmethod
    def login(usuario: str, password: str) -> tuple[bool, str]:
        """Valida credenciales contra `./farmacia login`.

        Returns:
            (True, mensaje) si stdout contiene 'OK' o 'LOGIN_EXITO'.
            (False, mensaje_error) en caso contrario.
        """
        usuario = (usuario or "").strip()
        password = (password or "").strip()

        if not usuario or not password:
            return False, "Ingrese usuario y contraseña."

        if not FARMACIA_BIN.exists():
            return False, f"No se encontró el binario: {FARMACIA_BIN}"

        try:
            resultado = subprocess.run(
                [str(FARMACIA_BIN), "login", usuario, password],
                capture_output=True,
                text=True,
                timeout=LOGIN_TIMEOUT_SEG,
                check=False,          # C retorna 0 aun con credenciales inválidas
                cwd=str(BASE_DIR),    # evita depender del cwd desde donde se lanza
            )
        except FileNotFoundError:
            return False, "Binario 'farmacia' no ejecutable o no encontrado."
        except subprocess.TimeoutExpired:
            return False, "El backend no respondió (timeout)."
        except Exception as exc:  # noqa: BLE001 - queremos mostrar cualquier fallo
            return False, f"Error al llamar al backend: {exc}"

        salida = (resultado.stdout or "").strip()
        # Contrato actual de main.c: "OK;login_correcto" / "ERROR;...".
        # Se acepta también "LOGIN_EXITO" para compatibilidad futura.
        salida_upper = salida.upper()
        if "OK" in salida_upper or "LOGIN_EXITO" in salida_upper:
            return True, salida or "Login exitoso."
        return False, salida or "Credenciales inválidas."

    @staticmethod
    def listar() -> tuple[bool, list[Producto] | str]:
        """Ejecuta `./farmacia listar` y parsea su salida.

        Contrato C (main.c::stub_listar):
            OK;total=N
            id;nombre;precio;stock;lote;fecha_vencimiento
            ...

        Returns:
            (True, [Producto, ...]) en éxito (lista vacía si total=0).
            (False, mensaje_error) si el binario falla o la salida es inválida.
            Las filas corruptas se omiten sin abortar el parseo.
        """
        if not FARMACIA_BIN.exists():
            return False, f"No se encontró el binario: {FARMACIA_BIN}"

        try:
            resultado = subprocess.run(
                [str(FARMACIA_BIN), "listar"],
                capture_output=True,
                text=True,
                timeout=LOGIN_TIMEOUT_SEG,
                check=False,
                cwd=str(BASE_DIR),
            )
        except FileNotFoundError:
            return False, "Binario 'farmacia' no ejecutable o no encontrado."
        except subprocess.TimeoutExpired:
            return False, "El backend no respondió (timeout)."
        except Exception as exc:  # noqa: BLE001
            return False, f"Error al llamar al backend: {exc}"

        salida = (resultado.stdout or "").strip()
        if not salida:
            return False, "El backend no devolvió datos."
        return BackendService.parsear_listar(salida)

    @staticmethod
    def parsear_listar(salida: str) -> tuple[bool, list[Producto] | str]:
        """Parsea el texto crudo de `./farmacia listar` a objetos Producto."""
        lineas = [ln.strip() for ln in salida.splitlines() if ln.strip()]
        if not lineas:
            return False, "El backend no devolvió datos."

        cabecera = lineas[0].upper()
        if cabecera.startswith("ERROR"):
            return False, lineas[0]
        if not cabecera.startswith("OK"):
            return False, f"Respuesta inesperada del backend: {lineas[0]}"

        productos: list[Producto] = []
        for linea in lineas[1:]:
            partes = linea.split(";")
            if len(partes) != 6:
                continue  # fila corrupta: se omite
            id_txt, nombre, precio_txt, stock_txt, lote, venc = (
                p.strip() for p in partes
            )
            try:
                productos.append(
                    Producto(
                        id=int(id_txt),
                        nombre=nombre,
                        precio=float(precio_txt),
                        stock=int(stock_txt),
                        lote=lote,
                        vencimiento=venc,
                    )
                )
            except ValueError:
                continue  # número inválido: se omite la fila
        return True, productos

    @staticmethod
    def vender(id_txt: str, cantidad_txt: str) -> tuple[bool, str]:
        """Ejecuta `./farmacia vender <id> <cantidad>`.

        Contrato C (main.c::stub_vender):
            OK;venta;id=<id>;cantidad=<n>;stock_restante=<s>
            ERROR;stock_insuficiente | producto_no_encontrado
              | parametros_invalidos | parametros_faltantes

        Returns:
            (True, mensaje_éxito) si stdout empieza con 'OK'.
            (False, mensaje_error_amigable) en caso contrario.
        """
        id_s = (id_txt or "").strip()
        cant_s = (cantidad_txt or "").strip()

        if not id_s or not cant_s:
            return False, "Ingrese el ID del producto y la cantidad."

        try:
            id_val = int(id_s)
            cant_val = int(cant_s)
        except ValueError:
            return False, "El ID y la cantidad deben ser números enteros."

        if id_val <= 0 or cant_val <= 0:
            return False, "El ID y la cantidad deben ser mayores a cero."

        if not FARMACIA_BIN.exists():
            return False, f"No se encontró el binario: {FARMACIA_BIN}"

        try:
            resultado = subprocess.run(
                [str(FARMACIA_BIN), "vender", str(id_val), str(cant_val)],
                capture_output=True,
                text=True,
                timeout=LOGIN_TIMEOUT_SEG,
                check=False,
                cwd=str(BASE_DIR),
            )
        except FileNotFoundError:
            return False, "Binario 'farmacia' no ejecutable o no encontrado."
        except subprocess.TimeoutExpired:
            return False, "El backend no respondió (timeout)."
        except Exception as exc:  # noqa: BLE001
            return False, f"Error al llamar al backend: {exc}"

        salida = (resultado.stdout or "").strip()
        if not salida:
            return False, "El backend no devolvió respuesta."

        if salida.upper().startswith("OK"):
            return True, BackendService._formatear_venta_ok(salida)

        salida_lower = salida.lower()
        if "stock_insuficiente" in salida_lower:
            return False, "Stock insuficiente para esta venta."
        if "producto_no_encontrado" in salida_lower:
            return False, "Producto no encontrado. Verifique el ID."
        if "parametros" in salida_lower:
            return False, "ID o cantidad inválidos."
        return False, salida

    @staticmethod
    def _formatear_venta_ok(salida: str) -> str:
        """Convierte 'OK;venta;id=1;cantidad=2;stock_restante=8' en texto amigable."""
        datos: dict[str, str] = {}
        for parte in salida.split(";"):
            if "=" in parte:
                clave, valor = parte.split("=", 1)
                datos[clave.strip().lower()] = valor.strip()
        if "id" in datos and "cantidad" in datos:
            base = (
                f"Venta registrada: ID {datos['id']} · "
                f"cantidad {datos['cantidad']}"
            )
            if "stock_restante" in datos:
                base += f" · stock restante {datos['stock_restante']}"
            return base + "."
        return salida

    @staticmethod
    def reporte() -> tuple[bool, ReporteResumen | str]:
        """Ejecuta `./farmacia reporte` y devuelve estadísticas globales.

        Contrato C esperado (stub_reporte en main.c):
            OK;total=N;alertas=M;valorizado=X;stock_total=Y
            ALERTA;id;nombre;stock
            ...

        Fallback: si el binario no reconoce el comando
        (`ERROR;comando_no_reconocido`), se llama a `listar()` y los
        totales se calculan en Python (total, stock<10, sum precio*stock).

        Returns:
            (True, ReporteResumen) en éxito.
            (False, mensaje_error) si falla el binario o no hay datos.
        """
        if not FARMACIA_BIN.exists():
            return False, f"No se encontró el binario: {FARMACIA_BIN}"

        try:
            resultado = subprocess.run(
                [str(FARMACIA_BIN), "reporte"],
                capture_output=True,
                text=True,
                timeout=LOGIN_TIMEOUT_SEG,
                check=False,
                cwd=str(BASE_DIR),
            )
        except FileNotFoundError:
            return False, "Binario 'farmacia' no ejecutable o no encontrado."
        except subprocess.TimeoutExpired:
            return False, "El backend no respondió (timeout)."
        except Exception as exc:  # noqa: BLE001
            return False, f"Error al llamar al backend: {exc}"

        salida = (resultado.stdout or "").strip()
        if not salida:
            return False, "El backend no devolvió datos."

        if "comando_no_reconocido" in salida.lower():
            # Binario antiguo sin subcomando `reporte`: derivar de `listar`.
            return BackendService._reporte_desde_listar()

        return BackendService.parsear_reporte(salida)

    @staticmethod
    def parsear_reporte(salida: str) -> tuple[bool, ReporteResumen | str]:
        """Parsea el texto crudo de `./farmacia reporte` a ReporteResumen.

        Tolera cabeceras parciales (claves faltantes -> 0) y omite
        líneas ALERTA corruptas sin abortar. Si la cabecera indica
        ERROR, la propaga como fallo.
        """
        lineas = [ln.strip() for ln in salida.splitlines() if ln.strip()]
        if not lineas:
            return False, "El backend no devolvió datos."

        cabecera = lineas[0]
        cabecera_upper = cabecera.upper()
        if cabecera_upper.startswith("ERROR"):
            return False, cabecera
        if not cabecera_upper.startswith("OK"):
            return False, f"Respuesta inesperada del backend: {lineas[0]}"

        datos: dict[str, str] = {}
        for parte in cabecera.split(";")[1:]:
            if "=" in parte:
                clave, valor = parte.split("=", 1)
                datos[clave.strip().lower()] = valor.strip()

        def _entero(claves: tuple[str, ...], defecto: int = 0) -> int:
            for clave in claves:
                if clave in datos:
                    try:
                        return int(float(datos[clave]))
                    except ValueError:
                        return defecto
            return defecto

        def _flotante(claves: tuple[str, ...], defecto: float = 0.0) -> float:
            for clave in claves:
                if clave in datos:
                    try:
                        return float(datos[clave])
                    except ValueError:
                        return defecto
            return defecto

        detalle: list[tuple[int, str, int]] = []
        for linea in lineas[1:]:
            if not linea.upper().startswith("ALERTA"):
                continue
            partes = linea.split(";")
            if len(partes) != 4:
                continue  # línea corrupta: se omite
            _tag, id_txt, nombre, stock_txt = (p.strip() for p in partes)
            try:
                detalle.append((int(id_txt), nombre, int(stock_txt)))
            except ValueError:
                continue

        total = _entero(("total", "total_productos", "productos"))
        alertas = _entero(("alertas", "stock_bajo", "n_alertas"), defecto=len(detalle))
        valorizado = _flotante(("valorizado", "valorizado_total", "total_valorizado", "valor_total"))
        stock_total = _entero(("stock_total", "stock", "unidades"))

        return True, ReporteResumen(
            total_productos=total,
            alertas=alertas,
            valorizado=valorizado,
            stock_total=stock_total,
            detalle_alertas=detalle,
        )

    @staticmethod
    def _reporte_desde_listar() -> tuple[bool, ReporteResumen | str]:
        """Fallback: construye el ReporteResumen a partir de `./farmacia listar`."""
        ok, resultado = BackendService.listar()
        if not ok:
            assert isinstance(resultado, str)
            return False, resultado
        assert isinstance(resultado, list)
        productos: list[Producto] = resultado
        detalle = [(p.id, p.nombre, p.stock) for p in productos if p.stock_bajo]
        return True, ReporteResumen(
            total_productos=len(productos),
            alertas=len(detalle),
            valorizado=sum(p.precio * p.stock for p in productos),
            stock_total=sum(p.stock for p in productos),
            detalle_alertas=detalle,
        )


# ---------------------------------------------------------------------------
# LoginFrame
# ---------------------------------------------------------------------------
class LoginFrame(ctk.CTkFrame):
    """Pantalla inicial: usuario, contraseña y botón de ingreso."""

    def __init__(self, master: ctk.CTk, on_success: Callable[[str], None]) -> None:
        super().__init__(master, fg_color="transparent")
        self._on_success = on_success

        # Tarjeta centrada
        self.grid_rowconfigure(0, weight=1)
        self.grid_columnconfigure(0, weight=1)

        card = ctk.CTkFrame(self, width=380, corner_radius=16)
        card.grid(row=0, column=0, padx=20, pady=20)
        card.grid_columnconfigure(0, weight=1)

        ctk.CTkLabel(
            card, text="Farmacia Urbana", font=ctk.CTkFont(size=24, weight="bold")
        ).grid(row=0, column=0, padx=32, pady=(32, 4))
        ctk.CTkLabel(
            card, text="Ingrese sus credenciales", font=ctk.CTkFont(size=13)
        ).grid(row=1, column=0, padx=32, pady=(0, 20))

        ctk.CTkLabel(card, text="Usuario:", anchor="w").grid(
            row=2, column=0, padx=32, sticky="ew"
        )
        self.entry_usuario = ctk.CTkEntry(card, placeholder_text="ej. admin", width=300)
        self.entry_usuario.grid(row=3, column=0, padx=32, pady=(4, 12))
        self.entry_usuario.focus()

        ctk.CTkLabel(card, text="Contraseña:", anchor="w").grid(
            row=4, column=0, padx=32, sticky="ew"
        )
        self.entry_password = ctk.CTkEntry(
            card, placeholder_text="••••••", show="*", width=300
        )
        self.entry_password.grid(row=5, column=0, padx=32, pady=(4, 8))
        self.entry_password.bind("<Return>", lambda _e: self._on_ingresar())

        self.lbl_error = ctk.CTkLabel(card, text="", text_color="#e5484d", wraplength=300)
        self.lbl_error.grid(row=6, column=0, padx=32, pady=(4, 4))

        self.btn_ingresar = ctk.CTkButton(
            card, text="Ingresar", width=300, command=self._on_ingresar
        )
        self.btn_ingresar.grid(row=7, column=0, padx=32, pady=(12, 32))

    def _on_ingresar(self) -> None:
        """Flujo login: llama al puente y delega el cambio de pantalla al controlador."""
        usuario = self.entry_usuario.get()
        password = self.entry_password.get()

        self.lbl_error.configure(text="")
        self.btn_ingresar.configure(state="disabled", text="Verificando...")

        ok, mensaje = BackendService.login(usuario, password)

        if ok:
            # Éxito: el controlador destruye este frame y muestra el dashboard.
            # Se pasa el usuario ya saneado (sin espacios).
            self._on_success(usuario.strip())
        else:
            self.lbl_error.configure(text=mensaje)
            self.btn_ingresar.configure(state="normal", text="Ingresar")
            # Limpia solo la contraseña por seguridad, conserva el usuario.
            self.entry_password.delete(0, "end")


# ===========================================================================
# VISTAS DINÁMICAS DEL ÁREA CENTRAL
# ---------------------------------------------------------------------------
# Cómo expandirlas (cuando quieras crecer el proyecto):
#   1. Cada vista es un CTkFrame autónomo con un método opcional `recargar()`.
#   2. Para dividir en archivos: crea views/inventario_view.py, views/ventas_view.py,
#      views/reportes_view.py, mueve cada clase tal cual y haz:
#          from views.inventario_view import InventarioView  (etc.)
#      No hay que tocar DashboardFrame salvo los imports.
#   3. Para conectar con C: usa subprocess.run([str(FARMACIA_BIN), "listar"|"vender"|"alertas", ...])
#      igual que hace BackendService.login. Hazlo en hilos (threading) si la
#      consulta tarda, para no congelar la UI.
# ===========================================================================

class InventarioView(ctk.CTkFrame):
    """Inventario con CTkScrollableFrame puro (sin ttk).

    Compatible con modo oscuro/claro: todos los colores son tuplas
    (light, dark). Efecto cebra por fila y stock < 10 en rojo.
    Para dividir: mover esta clase a views/inventario_view.py.
    """

    # Pesos de las 6 columnas: ID | Nombre (expandible) | Precio | Stock | Lote | Venc.
    _PESOS_COLS = (1, 4, 1, 1, 1, 1)
    _COLOR_FILA_PAR = ("gray84", "gray26")
    _COLOR_FILA_IMPAR = ("gray92", "gray29")
    _COLOR_STOCK_BAJO = "#e5484d"

    def __init__(self, master: ctk.CTk) -> None:
        super().__init__(master, fg_color="transparent")
        self._productos: list[Producto] = []

        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(2, weight=1)

        # ---- Barra superior: título + contador + actualizar ----
        toolbar = ctk.CTkFrame(self, fg_color="transparent")
        toolbar.grid(row=0, column=0, sticky="ew", padx=8, pady=(8, 4))
        toolbar.grid_columnconfigure(1, weight=1)

        ctk.CTkLabel(
            toolbar, text="Inventario", font=ctk.CTkFont(size=20, weight="bold")
        ).grid(row=0, column=0, sticky="w")

        self.lbl_contador = ctk.CTkLabel(toolbar, text="— productos", font=ctk.CTkFont(size=13))
        self.lbl_contador.grid(row=0, column=1, sticky="w", padx=(12, 0))

        self.btn_actualizar = ctk.CTkButton(
            toolbar, text="Actualizar", width=120, command=self.cargar_datos
        )
        self.btn_actualizar.grid(row=0, column=2, sticky="e")

        # ---- Encabezado fijo de columnas ----
        header = ctk.CTkFrame(self, fg_color=("gray75", "gray25"), corner_radius=8)
        header.grid(row=1, column=0, sticky="ew", padx=8, pady=(0, 6))
        self._configurar_columnas(header)
        for col, texto in enumerate(("ID", "Nombre", "Precio", "Stock", "Lote", "Vencimiento")):
            ctk.CTkLabel(
                header, text=texto, font=ctk.CTkFont(size=12, weight="bold"),
                anchor="w", fg_color="transparent",
            ).grid(row=0, column=col, sticky="ew", padx=8, pady=6)

        # ---- Cuerpo scrollable: una fila (CTkFrame) por producto ----
        self.scroll = ctk.CTkScrollableFrame(self, fg_color="transparent")
        self.scroll.grid(row=2, column=0, sticky="nsew", padx=8)
        self.scroll.grid_columnconfigure(0, weight=1)

        # ---- Estado: mensajes de carga / vacío / error ----
        self.lbl_estado = ctk.CTkLabel(self, text="", font=ctk.CTkFont(size=13))
        self.lbl_estado.grid(row=3, column=0, sticky="ew", padx=8, pady=(6, 4))

        self.cargar_datos()

    # -- API pública --
    def cargar_datos(self) -> None:
        """Llama a `./farmacia listar` y refresca la vista."""
        self.btn_actualizar.configure(state="disabled", text="Cargando...")
        self.lbl_estado.configure(text="Cargando inventario...")
        self._limpiar_filas()

        ok, resultado = BackendService.listar()

        if not ok:
            self.lbl_contador.configure(text="— productos")
            self.lbl_estado.configure(text=f"Error: {resultado}")
            self.btn_actualizar.configure(state="normal", text="Actualizar")
            return

        assert isinstance(resultado, list)
        self._productos = resultado
        self._renderizar()
        self.btn_actualizar.configure(state="normal", text="Actualizar")

    def recargar(self) -> None:
        """Hook invocado por DashboardFrame.mostrar_vista al navegar aquí."""
        self.cargar_datos()

    # -- Render interno --
    def _configurar_columnas(self, frame: ctk.CTkFrame) -> None:
        for col, peso in enumerate(self._PESOS_COLS):
            frame.grid_columnconfigure(col, weight=peso)

    def _limpiar_filas(self) -> None:
        for hijo in self.scroll.winfo_children():
            hijo.destroy()

    def _renderizar(self) -> None:
        self._limpiar_filas()
        total = len(self._productos)
        self.lbl_contador.configure(
            text=f"{total} producto{'s' if total != 1 else ''}"
        )
        if total == 0:
            self.lbl_estado.configure(text="Sin productos en inventario.")
            return
        self.lbl_estado.configure(text="")

        for i, prod in enumerate(self._productos):
            color = self._COLOR_FILA_PAR if i % 2 == 0 else self._COLOR_FILA_IMPAR
            fila = ctk.CTkFrame(self.scroll, fg_color=color, corner_radius=8)
            fila.pack(fill="x", padx=2, pady=2)
            self._configurar_columnas(fila)

            stock_color = self._COLOR_STOCK_BAJO if prod.stock_bajo else None
            valores = (
                str(prod.id),
                prod.nombre,
                f"{prod.precio:.2f}",
                str(prod.stock),
                prod.lote,
                prod.vencimiento,
            )
            for col, valor in enumerate(valores):
                es_stock_bajo = col == 3 and prod.stock_bajo
                fuente = ctk.CTkFont(size=12, weight="bold" if es_stock_bajo else "normal")
                kwargs = {"text_color": stock_color} if es_stock_bajo else {}
                ctk.CTkLabel(
                    fila, text=valor, font=fuente,
                    anchor="w", fg_color="transparent", **kwargs,
                ).grid(row=0, column=col, sticky="ew", padx=8, pady=6)


class VentasView(ctk.CTkFrame):
    """Formulario de venta: ID + cantidad + botón que llama a `./farmacia vender`.

    Estética: título + CTkScrollableFrame + tarjeta centrada, mismos radios
    (corner_radius=12/16) y tipografías que LoginFrame/InventarioView.
    El mensaje de resultado usa verde para OK y rojo para ERROR.
    Para dividir: mover esta clase a views/ventas_view.py.
    """

    _COLOR_EXITO = "#2ea043"
    _COLOR_ERROR = "#e5484d"

    def __init__(self, master: ctk.CTk) -> None:
        super().__init__(master, fg_color="transparent")
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(1, weight=1)

        ctk.CTkLabel(
            self, text="Ventas", font=ctk.CTkFont(size=20, weight="bold")
        ).grid(row=0, column=0, sticky="w", padx=8, pady=(8, 0))
        ctk.CTkLabel(
            self,
            text="Registre una venta por ID de producto.",
            font=ctk.CTkFont(size=13),
        ).grid(row=0, column=0, sticky="w", padx=8, pady=(36, 0))

        # Contenedor scrollable (coherente con InventarioView, útil en ventanas bajas).
        self.scroll = ctk.CTkScrollableFrame(self, fg_color="transparent")
        self.scroll.grid(row=1, column=0, sticky="nsew", padx=8, pady=8)
        self.scroll.grid_columnconfigure(0, weight=1)

        card = ctk.CTkFrame(self.scroll, corner_radius=16)
        card.pack(padx=40, pady=20, fill="x")
        card.grid_columnconfigure(0, weight=1)

        ctk.CTkLabel(
            card, text="Registrar Venta", font=ctk.CTkFont(size=16, weight="bold")
        ).grid(row=0, column=0, padx=32, pady=(24, 4), sticky="w")

        ctk.CTkLabel(card, text="ID del producto:", anchor="w").grid(
            row=1, column=0, padx=32, sticky="ew"
        )
        self.entry_id = ctk.CTkEntry(card, placeholder_text="ej. 101")
        self.entry_id.grid(row=2, column=0, padx=32, pady=(4, 12), sticky="ew")
        self.entry_id.focus()

        ctk.CTkLabel(card, text="Cantidad:", anchor="w").grid(
            row=3, column=0, padx=32, sticky="ew"
        )
        self.entry_cantidad = ctk.CTkEntry(card, placeholder_text="ej. 2")
        self.entry_cantidad.grid(row=4, column=0, padx=32, pady=(4, 8), sticky="ew")

        self.lbl_resultado = ctk.CTkLabel(card, text="", wraplength=340)
        self.lbl_resultado.grid(row=5, column=0, padx=32, pady=(4, 4), sticky="ew")

        self.btn_registrar = ctk.CTkButton(
            card, text="Registrar Venta", command=self._on_registrar
        )
        self.btn_registrar.grid(row=6, column=0, padx=32, pady=(12, 24), sticky="ew")

        self.entry_id.bind("<Return>", lambda _e: self._on_registrar())
        self.entry_cantidad.bind("<Return>", lambda _e: self._on_registrar())

    def _on_registrar(self) -> None:
        """Valida, llama a BackendService.vender y muestra OK/ERROR."""
        self.lbl_resultado.configure(text="")
        self.btn_registrar.configure(state="disabled", text="Procesando...")

        ok, mensaje = BackendService.vender(
            self.entry_id.get(), self.entry_cantidad.get()
        )

        if ok:
            self.lbl_resultado.configure(text=mensaje, text_color=self._COLOR_EXITO)
            self.entry_id.delete(0, "end")
            self.entry_cantidad.delete(0, "end")
            self.entry_id.focus()
        else:
            self.lbl_resultado.configure(text=mensaje, text_color=self._COLOR_ERROR)
            self.entry_cantidad.delete(0, "end")

        self.btn_registrar.configure(state="normal", text="Registrar Venta")

    def recargar(self) -> None:
        """Hook llamado cada vez que se navega a esta vista."""
        self.lbl_resultado.configure(text="")
        self.entry_id.focus()


class ReportesView(ctk.CTkFrame):
    """Reportes globales: tarjetas resumen + listado de alertas stock bajo.

    Fuente: `BackendService.reporte()` (`./farmacia reporte` con
    fallback a `./farmacia listar`). Estética alineada con
    Inventario/Ventas: CTkScrollableFrame + tarjetas corner_radius=12,
    efecto cebra y stock bajo en rojo.
    Para dividir: mover esta clase a views/reportes_view.py.
    """

    _COLOR_FILA_PAR = ("gray84", "gray26")
    _COLOR_FILA_IMPAR = ("gray92", "gray29")
    _COLOR_STOCK_BAJO = "#e5484d"
    _COLOR_TEXTO_DEFAULT = ("gray10", "gray90")
    _COLOR_VALOR = "#2ea043"

    def __init__(self, master: ctk.CTk) -> None:
        super().__init__(master, fg_color="transparent")
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(2, weight=1)

        # ---- Barra superior: título + actualizar ----
        toolbar = ctk.CTkFrame(self, fg_color="transparent")
        toolbar.grid(row=0, column=0, sticky="ew", padx=8, pady=(8, 4))
        toolbar.grid_columnconfigure(1, weight=1)

        ctk.CTkLabel(
            toolbar, text="Reportes", font=ctk.CTkFont(size=20, weight="bold")
        ).grid(row=0, column=0, sticky="w")

        self.btn_actualizar = ctk.CTkButton(
            toolbar, text="Actualizar Reporte", width=150, command=self.cargar_datos
        )
        self.btn_actualizar.grid(row=0, column=2, sticky="e")

        # ---- Estado: carga / error ----
        self.lbl_estado = ctk.CTkLabel(self, text="", font=ctk.CTkFont(size=13))
        self.lbl_estado.grid(row=1, column=0, sticky="ew", padx=8, pady=(0, 4))

        # ---- Cuerpo scrollable ----
        self.scroll = ctk.CTkScrollableFrame(self, fg_color="transparent")
        self.scroll.grid(row=2, column=0, sticky="nsew", padx=8)
        self.scroll.grid_columnconfigure(0, weight=1)

        # ---- Fila de tarjetas resumen (2x2, corner_radius=12) ----
        self.frame_tarjetas = ctk.CTkFrame(self.scroll, fg_color="transparent")
        self.frame_tarjetas.pack(fill="x", padx=2, pady=(2, 8))
        self.frame_tarjetas.grid_columnconfigure((0, 1), weight=1)

        self.lbl_total_valor = self._crear_tarjeta(0, 0, "Total de productos")
        self.lbl_alertas_valor = self._crear_tarjeta(0, 1, "Alertas de stock bajo")
        self.lbl_valorizado_valor = self._crear_tarjeta(1, 0, "Valorizado total")
        self.lbl_stock_valor = self._crear_tarjeta(1, 1, "Stock total (unidades)")

        # ---- Tarjeta de alertas ----
        self.card_alertas = ctk.CTkFrame(self.scroll, corner_radius=12)
        self.card_alertas.pack(fill="x", padx=2, pady=2)
        self.card_alertas.grid_columnconfigure(0, weight=1)

        ctk.CTkLabel(
            self.card_alertas,
            text="Alertas de stock bajo (< 10)",
            font=ctk.CTkFont(size=14, weight="bold"),
        ).grid(row=0, column=0, sticky="w", padx=16, pady=(14, 2))
        self.lbl_alertas_sub = ctk.CTkLabel(
            self.card_alertas, text="—", font=ctk.CTkFont(size=12)
        )
        self.lbl_alertas_sub.grid(row=1, column=0, sticky="w", padx=16, pady=(0, 8))

        self.frame_filas = ctk.CTkFrame(self.card_alertas, fg_color="transparent")
        self.frame_filas.grid(row=2, column=0, sticky="ew", padx=8, pady=(0, 12))
        self.frame_filas.grid_columnconfigure(0, weight=1)

        self.cargar_datos()

    # -- API pública --
    def cargar_datos(self) -> None:
        """Invoca al backend C y refresca tarjetas + alertas."""
        self.btn_actualizar.configure(state="disabled", text="Actualizando...")
        self.lbl_estado.configure(text="Cargando reporte...")
        try:
            ok, resultado = BackendService.reporte()
        except Exception as exc:  # noqa: BLE001 - no congelar la UI
            self.lbl_estado.configure(text=f"Error: {exc}")
            self.btn_actualizar.configure(state="normal", text="Actualizar Reporte")
            return

        if not ok:
            assert isinstance(resultado, str)
            self.lbl_estado.configure(text=f"Error: {resultado}")
            self.btn_actualizar.configure(state="normal", text="Actualizar Reporte")
            return

        assert isinstance(resultado, ReporteResumen)
        self._renderizar(resultado)
        self.lbl_estado.configure(text="")
        self.btn_actualizar.configure(state="normal", text="Actualizar Reporte")

    def recargar(self) -> None:
        """Hook invocado por DashboardFrame.mostrar_vista al navegar aquí."""
        self.cargar_datos()

    # -- Render interno --
    def _crear_tarjeta(self, fila: int, col: int, titulo: str) -> ctk.CTkLabel:
        """Crea una tarjeta resumen y devuelve su label de valor."""
        card = ctk.CTkFrame(self.frame_tarjetas, corner_radius=12)
        card.grid(row=fila, column=col, sticky="nsew", padx=6, pady=6)
        card.grid_columnconfigure(0, weight=1)
        ctk.CTkLabel(card, text=titulo, font=ctk.CTkFont(size=12)).grid(
            row=0, column=0, sticky="w", padx=16, pady=(14, 2)
        )
        lbl_valor = ctk.CTkLabel(card, text="—", font=ctk.CTkFont(size=24, weight="bold"))
        lbl_valor.grid(row=1, column=0, sticky="w", padx=16, pady=(0, 14))
        return lbl_valor

    def _renderizar(self, resumen: ReporteResumen) -> None:
        self.lbl_total_valor.configure(text=str(resumen.total_productos))
        self.lbl_alertas_valor.configure(
            text=str(resumen.alertas),
            text_color=self._COLOR_STOCK_BAJO if resumen.alertas > 0 else self._COLOR_TEXTO_DEFAULT,
        )
        self.lbl_valorizado_valor.configure(f"${resumen.valorizado:,.2f}")
        self.lbl_stock_valor.configure(text=str(resumen.stock_total))

        detalle = resumen.detalle_alertas or []
        if not detalle:
            self.lbl_alertas_sub.configure(text="Sin alertas. Todo el stock está OK.")
        else:
            self.lbl_alertas_sub.configure(
                text=f"{len(detalle)} producto(s) con stock bajo."
            )

        for hijo in self.frame_filas.winfo_children():
            hijo.destroy()

        for i, (pid, nombre, stock) in enumerate(detalle):
            color = self._COLOR_FILA_PAR if i % 2 == 0 else self._COLOR_FILA_IMPAR
            fila_frame = ctk.CTkFrame(self.frame_filas, fg_color=color, corner_radius=8)
            fila_frame.pack(fill="x", padx=2, pady=2)
            fila_frame.grid_columnconfigure(1, weight=1)
            ctk.CTkLabel(
                fila_frame, text=str(pid), font=ctk.CTkFont(size=12, weight="bold"),
                fg_color="transparent",
            ).grid(row=0, column=0, sticky="w", padx=8, pady=6)
            ctk.CTkLabel(
                fila_frame, text=nombre, font=ctk.CTkFont(size=12),
                fg_color="transparent", anchor="w",
            ).grid(row=0, column=1, sticky="ew", padx=8, pady=6)
            ctk.CTkLabel(
                fila_frame, text=f"stock: {stock}",
                font=ctk.CTkFont(size=12, weight="bold"),
                text_color=self._COLOR_STOCK_BAJO, fg_color="transparent",
            ).grid(row=0, column=2, sticky="e", padx=8, pady=6)


# ---------------------------------------------------------------------------
# DashboardFrame: sidebar + área central dinámica
# ---------------------------------------------------------------------------
class DashboardFrame(ctk.CTkFrame):
    """Panel principal tras login exitoso."""

    def __init__(
        self, master: ctk.CTk, usuario: str, on_logout: Callable[[], None]
    ) -> None:
        super().__init__(master, fg_color="transparent")
        self._on_logout = on_logout
        self._vista_actual: ctk.CTkFrame | None = None
        self._botones: dict[str, ctk.CTkButton] = {}

        # Layout: columna 0 = sidebar fija, columna 1 = contenido expandible.
        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(0, weight=1)

        # ---- Sidebar izquierda ----
        sidebar = ctk.CTkFrame(self, width=220, corner_radius=0)
        sidebar.grid(row=0, column=0, sticky="nsew")
        sidebar.grid_rowconfigure(4, weight=1)  # empuja "Salir" abajo

        ctk.CTkLabel(
            sidebar, text="Farmacia Urbana", font=ctk.CTkFont(size=18, weight="bold")
        ).grid(row=0, column=0, padx=20, pady=(24, 4))
        ctk.CTkLabel(sidebar, text=f"Usuario: {usuario}").grid(
            row=1, column=0, padx=20, pady=(0, 20)
        )

        # Mapa navegación: clave -> (texto botón, clase vista).
        # Para agregar una pantalla nueva: añade la clase arriba y una entrada aquí.
        self._rutas: dict[str, tuple[str, type[ctk.CTkFrame]]] = {
            "inventario": ("Inventario", InventarioView),
            "ventas": ("Ventas", VentasView),
            "reportes": ("Reportes", ReportesView),
        }

        fila = 2
        for clave, (texto, _vista) in self._rutas.items():
            btn = ctk.CTkButton(
                sidebar,
                text=texto,
                anchor="w",
                fg_color="transparent",
                text_color=("gray10", "gray90"),
                hover_color=("gray70", "gray30"),
                command=lambda c=clave: self.mostrar_vista(c),
            )
            btn.grid(row=fila, column=0, padx=16, pady=6, sticky="ew")
            self._botones[clave] = btn
            fila += 1

        ctk.CTkButton(
            sidebar, text="Cerrar sesión", fg_color="#e5484d", hover_color="#c93a3e",
            command=self._on_logout,
        ).grid(row=5, column=0, padx=16, pady=20, sticky="ew")

        # ---- Área central dinámica ----
        self.contenedor = ctk.CTkFrame(self, fg_color="transparent", corner_radius=12)
        self.contenedor.grid(row=0, column=1, sticky="nsew", padx=16, pady=16)
        self.contenedor.grid_columnconfigure(0, weight=1)
        self.contenedor.grid_rowconfigure(0, weight=1)

        self.mostrar_vista("inventario")  # vista por defecto

    def mostrar_vista(self, nombre: str) -> None:
        """Destruye la vista actual e instancia la nueva en el contenedor."""
        if nombre not in self._rutas:
            return

        # 1. Limpiar área central.
        for hijo in self.contenedor.winfo_children():
            hijo.destroy()

        # 2. Instanciar la vista pedida.
        _texto, clase_vista = self._rutas[nombre]
        self._vista_actual = clase_vista(self.contenedor)
        self._vista_actual.grid(row=0, column=0, sticky="nsew")

        # 3. Hook opcional de recarga (útil cuando conectes el backend C).
        if hasattr(self._vista_actual, "recargar"):
            self._vista_actual.recargar()  # type: ignore[attr-defined]

        # 4. Resaltar botón activo.
        for clave, boton in self._botones.items():
            if clave == nombre:
                boton.configure(fg_color=("gray75", "gray25"))
            else:
                boton.configure(fg_color="transparent")


# ---------------------------------------------------------------------------
# FarmaciaApp: controlador raíz, dueño del cambio de pantallas
# ---------------------------------------------------------------------------
class FarmaciaApp(ctk.CTk):
    """Ventana principal. Orquesta Login <-> Dashboard."""

    def __init__(self) -> None:
        super().__init__()
        self.title("Farmacia Urbana")
        self.geometry("1100x650")
        self.minsize(900, 550)

        self.usuario_actual: str | None = None
        self.current_frame: ctk.CTkFrame | None = None

        self.mostrar_login()

    # -- cambio de pantallas --
    def _cambiar_frame(self, nuevo: ctk.CTkFrame) -> None:
        if self.current_frame is not None:
            self.current_frame.destroy()  # destroy (no pack_forget) limpia credenciales
        self.current_frame = nuevo
        self.current_frame.pack(fill="both", expand=True)

    def mostrar_login(self) -> None:
        """Muestra (o restaura) la pantalla de login."""
        self.usuario_actual = None
        self._cambiar_frame(LoginFrame(self, on_success=self.mostrar_dashboard))

    def mostrar_dashboard(self, usuario: str) -> None:
        """Oculta login y muestra el dashboard tras un login exitoso."""
        self.usuario_actual = usuario
        self._cambiar_frame(
            DashboardFrame(self, usuario=usuario, on_logout=self.mostrar_login)
        )


def main() -> None:
    app = FarmaciaApp()
    app.mainloop()


if __name__ == "__main__":
    main()
