"""Farmacia Urbana — frontend CustomTkinter (app_farmacia.py).

Estructura base:
    FarmaciaApp (controlador raiz / ctk.CTk)
      |- LoginFrame        -> autenticacion via ConexionC.ejecutar("login", ...)
      |- DashboardFrame   -> sidebar lateral + area central con vistas

Backend: binario C ./farmacia, IPC normalizada por integracion_ipc.ConexionC.
"""

from __future__ import annotations

import datetime
import os
import tkinter as tk
from pathlib import Path
from tkinter import messagebox, ttk
from typing import Callable

import customtkinter as ctk

from integracion_ipc import ConexionC


ctk.set_appearance_mode("dark")
ctk.set_default_color_theme("blue")


# ===========================================================================
# LoginFrame
# ===========================================================================
class LoginFrame(ctk.CTkFrame):
    """Pantalla inicial: usuario, contrasena y boton de ingreso."""

    def __init__(
        self,
        master: ctk.CTk,
        on_success: Callable[[str, str, str, str], None],
    ) -> None:
        super().__init__(master, fg_color="transparent")
        self._on_success = on_success

        self.grid_rowconfigure(0, weight=1)
        self.grid_columnconfigure(0, weight=1)

        # Tarjeta centrada
        card = ctk.CTkFrame(self, width=380, corner_radius=16)
        card.grid(row=0, column=0, padx=20, pady=20)
        card.grid_rowconfigure(0, weight=1)
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

        ctk.CTkLabel(card, text="Contrasena:", anchor="w").grid(
            row=4, column=0, padx=32, sticky="ew"
        )
        self.entry_password = ctk.CTkEntry(
            card, placeholder_text="••••••", show="*", width=300
        )
        self.entry_password.grid(row=5, column=0, padx=32, pady=(4, 8))
        self.entry_password.bind("<Return>", lambda _e: self._on_ingresar())

        self.lbl_error = ctk.CTkLabel(
            card, text="", text_color="#e5484d", wraplength=300
        )
        self.lbl_error.grid(row=6, column=0, padx=32, pady=(4, 4))

        self.btn_ingresar = ctk.CTkButton(
            card, text="Ingresar", width=300, command=self._on_ingresar
        )
        self.btn_ingresar.grid(row=7, column=0, padx=32, pady=(12, 32))

    def _on_ingresar(self) -> None:
        """Valida contra el backend C y delega al dashboard o muestra error."""
        usuario = self.entry_usuario.get().strip()
        password = self.entry_password.get().strip()

        self.lbl_error.configure(text="")
        self.btn_ingresar.configure(state="disabled", text="Verificando...")

        respuesta = ConexionC.ejecutar("login", usuario, password)

        self.btn_ingresar.configure(state="normal", text="Ingresar")

        if respuesta.get("status") == "OK":
            # Respuesta del backend:
            # LOGIN_OK;<rol>;<nombre>;<id_sucursal>;<nombre_sucursal>.
            rol, id_sucursal, nombre_sucursal = self._extraer_sesion(respuesta)
            # Éxito: el controlador destruye este frame y muestra el dashboard.
            self._on_success(usuario, rol, id_sucursal, nombre_sucursal)
            return

        # ERROR o FATAL: se muestra el mensaje en el label rojo.
        mensaje = respuesta.get("mensaje") or "No se pudo completar el inicio de sesion."
        self.lbl_error.configure(text=mensaje)

    @staticmethod
    def _extraer_sesion(respuesta: dict) -> tuple[str, str, str]:
        """Extrae (rol, id_sucursal, nombre_sucursal) del LOGIN_OK."""
        for fila in respuesta.get("data") or []:
            valores = [str(v).strip() for v in fila]
            if valores and valores[0].upper() in {"LOGIN_OK", "LOGIN"}:
                valores = valores[1:]
            if not valores:
                continue

            rol = "VENDEDOR"
            rol_txt = valores[0].upper()
            if rol_txt.startswith("ADMIN"):
                rol = "ADMIN"
            elif rol_txt.startswith("TECN"):
                rol = "TECNICO"
            elif rol_txt.startswith("VEND"):
                rol = "VENDEDOR"

            id_sucursal = valores[2] if len(valores) > 2 else "1"
            if not id_sucursal.isdigit() or int(id_sucursal) <= 0:
                id_sucursal = "1"
            nombre_sucursal = (
                valores[3] if len(valores) > 3 and valores[3] else "Sede Central"
            )
            return rol, id_sucursal, nombre_sucursal

        return "VENDEDOR", "1", "Sede Central"


# ===========================================================================
# VistaInventario (búsqueda de productos)
# ===========================================================================
class VistaInventario(ctk.CTkFrame):
    """Vista de inventario: búsqueda + CRUD (mantenimiento solo para ADMIN).

    Cualquier rol distinto de ADMIN (TECNICO, VENDEDOR, etc.) puede consultar
    el inventario (búsqueda, tabla y refresco), pero los botones de
    mantenimiento quedan deshabilitados y sus métodos bloqueados: la vista
    opera en modo lectura.

    Contrato de `buscar_prod`:
        [Código, Nombre, Precio, Stock, Vencimiento, Categoría]
    El índice 4 es siempre la fecha de vencimiento y el 5 la categoría.
    """

    COLUMNAS = ("Código", "Nombre", "Precio", "Stock", "Vencimiento", "Categoría")
    ANCHOS = {
        "Código": 110,
        "Nombre": 200,
        "Precio": 90,
        "Stock": 80,
        "Vencimiento": 110,
        "Categoría": 110,
    }
    CATEGORIAS = ("Medicamento", "Crema", "Jarabe", "Accesorio", "Higiene")

    def __init__(
        self,
        master: ctk.CTk,
        rol: str = "VENDEDOR",
        es_admin: bool | None = None,
    ) -> None:
        super().__init__(master, fg_color="transparent")
        # El rol de la sesión define los permisos de mantenimiento. Se acepta
        # también `es_admin` por compatibilidad con llamadas antiguas.
        rol_normalizado = (rol or "VENDEDOR").strip().upper()
        if es_admin is not None:
            if es_admin:
                rol_normalizado = "ADMIN"
            elif rol_normalizado.startswith("ADMIN"):
                rol_normalizado = "VENDEDOR"
        self.rol = rol_normalizado
        # Solo el rol ADMIN posee permisos de escritura; cualquier otro rol
        # (TECNICO, VENDEDOR, etc.) queda en modo lectura.
        self.es_admin = self.rol == "ADMIN"
        self.puede_mantenimiento = self.es_admin
        self.entry_codigo: ctk.CTkEntry | None = None
        self.entry_nombre: ctk.CTkEntry | None = None
        self.entry_precio: ctk.CTkEntry | None = None
        self.entry_stock: ctk.CTkEntry | None = None
        self.entry_lote: ctk.CTkEntry | None = None
        self.entry_vencimiento: ctk.CTkEntry | None = None
        self.combo_categoria: ctk.CTkComboBox | None = None
        self.btn_crear: ctk.CTkButton | None = None
        self.btn_editar: ctk.CTkButton | None = None
        self.btn_eliminar: ctk.CTkButton | None = None

        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(0, weight=0)
        self.grid_rowconfigure(1, weight=0)
        self.grid_rowconfigure(2, weight=1)
        self.grid_rowconfigure(3, weight=0)

        # ---- Barra superior: busqueda ----
        barra = ctk.CTkFrame(self, fg_color="transparent")
        barra.grid(row=0, column=0, sticky="ew", padx=8, pady=(8, 4))
        barra.grid_columnconfigure(0, weight=1)

        self.entry_busqueda = ctk.CTkEntry(
            barra, placeholder_text="Buscar producto por código o nombre..."
        )
        self.entry_busqueda.grid(row=0, column=0, sticky="ew", padx=(0, 8))
        self.entry_busqueda.bind("<Return>", lambda _e: self._on_buscar())

        self.btn_buscar = ctk.CTkButton(
            barra, text="Buscar", width=120, command=self._on_buscar
        )
        self.btn_buscar.grid(row=0, column=1)

        # Refresco de la tabla: disponible para TODOS los roles (incluido TECNICO).
        self.btn_refrescar = ctk.CTkButton(
            barra, text="Refrescar", width=110, command=self._on_buscar
        )
        self.btn_refrescar.grid(row=0, column=2, padx=(8, 0))

        # ---- Formulario de mantenimiento (CRUD) ----
        # Se construye para todos los roles; los botones de escritura se
        # deshabilitan para todo rol que no sea ADMIN en _aplicar_permisos().
        self._crear_formulario()
        self._aplicar_permisos()

        # ---- Tabla (Treeview con scroll) ----
        frame_tabla = ctk.CTkFrame(
            self, fg_color=("gray75", "gray25"), corner_radius=12
        )
        frame_tabla.grid(row=2, column=0, sticky="nsew", padx=8, pady=8)
        frame_tabla.grid_rowconfigure(0, weight=1)
        frame_tabla.grid_columnconfigure(0, weight=1)

        self.tree = ttk.Treeview(frame_tabla, columns=self.COLUMNAS, show="headings")
        scrollbar = ttk.Scrollbar(
            frame_tabla, orient="vertical", command=self.tree.yview
        )
        self.tree.configure(yscrollcommand=scrollbar.set)
        self.tree.grid(row=0, column=0, sticky="nsew")
        scrollbar.grid(row=0, column=1, padx=(0, 4), sticky="ns")

        for col in self.COLUMNAS:
            self.tree.heading(col, text=col, anchor="w")
            self.tree.column(col, width=self.ANCHOS[col], anchor="w")

        self._configurar_estilo_treeview()
        if self.es_admin:
            self.tree.bind("<<TreeviewSelect>>", self._on_seleccionar)

        # ---- Estado ----
        self.lbl_estado = ctk.CTkLabel(
            self, text="", font=ctk.CTkFont(size=13), text_color="#e5484d"
        )
        self.lbl_estado.grid(row=3, column=0, sticky="ew", padx=8, pady=(0, 6))

        # Al abrir la vista se carga el inventario inicial (primeros 200 productos).
        self._on_buscar()

    def _crear_formulario(self) -> None:
        form = ctk.CTkFrame(self, fg_color="transparent")
        form.grid(row=1, column=0, sticky="ew", padx=8, pady=(0, 4))

        self.entry_codigo = ctk.CTkEntry(
            form, placeholder_text="Código (vacío = AUTO)"
        )
        self.entry_nombre = ctk.CTkEntry(form, placeholder_text="Nombre")
        self.combo_categoria = ctk.CTkComboBox(
            form, values=list(self.CATEGORIAS), state="readonly"
        )
        self.combo_categoria.set(self.CATEGORIAS[0])
        self.entry_precio = ctk.CTkEntry(form, placeholder_text="Precio")
        self.entry_stock = ctk.CTkEntry(form, placeholder_text="Stock (ej. 0.5)")
        self.entry_lote = ctk.CTkEntry(form, placeholder_text="Lote (opcional)")
        self.entry_vencimiento = ctk.CTkEntry(
            form, placeholder_text="Vencimiento (YYYY-MM-DD)"
        )

        campos = (
            ("Código", self.entry_codigo),
            ("Nombre", self.entry_nombre),
            ("Categoría", self.combo_categoria),
            ("Precio", self.entry_precio),
            ("Stock", self.entry_stock),
            ("Lote", self.entry_lote),
            ("Vencimiento", self.entry_vencimiento),
        )
        for col, (titulo, entrada) in enumerate(campos):
            form.grid_columnconfigure(col, weight=1)
            ctk.CTkLabel(form, text=titulo, anchor="w").grid(
                row=0, column=col, sticky="ew", padx=(0, 6)
            )
            entrada.grid(row=1, column=col, sticky="ew", padx=(0, 6))

        self.btn_crear = ctk.CTkButton(
            form, text="Crear", width=110, command=self._on_crear,
            fg_color="#2ea043", hover_color="#258a3d",
        )
        self.btn_crear.grid(row=2, column=0, sticky="w", pady=(8, 0))

        self.btn_editar = ctk.CTkButton(
            form, text="Editar", width=110, command=self._on_editar,
            fg_color="#2f6feb", hover_color="#2658c9",
        )
        self.btn_editar.grid(row=2, column=1, sticky="w", pady=(8, 0))

        self.btn_eliminar = ctk.CTkButton(
            form, text="Eliminar", width=110, command=self._on_eliminar,
            fg_color="#e5484d", hover_color="#c93a3e",
        )
        self.btn_eliminar.grid(row=2, column=2, sticky="w", pady=(8, 0))

        self.btn_limpiar = ctk.CTkButton(
            form, text="Limpiar", width=110, command=self._limpiar_formulario,
            fg_color="gray40", hover_color="gray30",
        )
        self.btn_limpiar.grid(row=2, column=3, sticky="w", pady=(8, 0))

    def _aplicar_permisos(self) -> None:
        """Habilita el mantenimiento solo para ADMIN.

        Los roles TECNICO y VENDEDOR conservan la búsqueda, la tabla y el
        botón Refrescar, pero los botones Crear/Editar/Eliminar quedan
        visualmente deshabilitados (y sus métodos bloqueados).
        """
        estado = "normal" if self.puede_mantenimiento else "disabled"
        for boton in (self.btn_crear, self.btn_editar, self.btn_eliminar):
            if boton is not None:
                boton.configure(state=estado)

    # -- Estilo Treeview para tema oscuro de CustomTkinter --
    def _configurar_estilo_treeview(self) -> None:
        estilo = ttk.Style()
        estilo.theme_use("clam")

        estilo.configure(
            "Treeview",
            background="#343541",
            foreground="#DCE4EE",
            fieldbackground="#343541",
            rowheight=28,
            font=("Roboto", 12),
        )
        estilo.map(
            "Treeview",
            background=[("selected", "#4F4F4F")],
            foreground=[("selected", "#FFFFFF")],
        )
        estilo.configure(
            "Treeview.Heading",
            background="#343541",
            foreground="#DCE4EE",
            font=("Roboto", 11, "bold"),
            relief="flat",
        )
        estilo.map(
            "Treeview.Heading",
            background=[("active", "#3F3F4B")],
        )

    # -- Accion del boton Buscar --
    def _on_buscar(self) -> None:
        texto = self.entry_busqueda.get().strip()
        respuesta = ConexionC.ejecutar("buscar_prod", texto)

        if respuesta.get("status") == "OK":
            datos = respuesta.get("data") or []
            self.tree.delete(*self.tree.get_children())
            for fila in datos:
                self.tree.insert("", "end", values=self._normalizar_fila(fila))
            self._mostrar_estado("", ok=True)
        else:
            self.tree.delete(*self.tree.get_children())
            mensaje = respuesta.get("mensaje") or "No se encontraron resultados."
            self._mostrar_estado(f"Error: {mensaje}", ok=False)

    # -- CRUD --
    def _on_seleccionar(self, _event=None) -> None:
        seleccion = self.tree.selection()
        if not seleccion or not self.puede_mantenimiento:
            return
        valores = self.tree.item(seleccion[0], "values")
        if not valores:
            return
        self._poner_texto(self.entry_codigo, valores[0] if len(valores) > 0 else "")
        self._poner_texto(self.entry_nombre, valores[1] if len(valores) > 1 else "")
        self._poner_texto(self.entry_precio, valores[2] if len(valores) > 2 else "")
        self._poner_texto(self.entry_stock, valores[3] if len(valores) > 3 else "")
        self._poner_texto(
            self.entry_vencimiento, valores[4] if len(valores) > 4 else ""
        )
        if self.combo_categoria is not None:
            categoria = (
                str(valores[5]).strip()
                if len(valores) > 5 and str(valores[5]).strip()
                else self.CATEGORIAS[0]
            )
            self.combo_categoria.set(categoria)
        self._poner_texto(self.entry_lote, "")

    def _leer_formulario(self) -> tuple[str, str, float, float, str, str, str] | None:
        if not all(
            (self.entry_codigo, self.entry_nombre, self.entry_precio,
             self.entry_stock, self.entry_vencimiento)
        ):
            return None

        codigo = self.entry_codigo.get().strip()
        nombre = self.entry_nombre.get().strip()
        precio_txt = self.entry_precio.get().strip().replace(",", ".")
        stock_txt = self.entry_stock.get().strip().replace(",", ".")
        lote = self.entry_lote.get().strip() if self.entry_lote else ""
        fecha = self.entry_vencimiento.get().strip()
        categoria = (
            self.combo_categoria.get().strip()
            if self.combo_categoria is not None
            else self.CATEGORIAS[0]
        )

        # El código es opcional: en el alta se envía "AUTO" y el backend C
        # genera un EAN-13 peruano (775...) antes de insertar el producto.
        if not nombre:
            self._mostrar_estado("El nombre es obligatorio.", ok=False)
            return None

        try:
            precio = float(precio_txt)
            stock = float(stock_txt)
        except ValueError:
            self._mostrar_estado("Precio y stock deben ser numéricos.", ok=False)
            return None

        if precio < 0 or stock < 0:
            self._mostrar_estado("Precio y stock no pueden ser negativos.", ok=False)
            return None

        if fecha:
            try:
                datetime.date.fromisoformat(fecha)
            except ValueError:
                self._mostrar_estado(
                    "El vencimiento debe tener formato YYYY-MM-DD.", ok=False
                )
                return None

        return codigo, nombre, precio, stock, lote, fecha, categoria

    def _on_crear(self) -> None:
        if self._sin_permiso_mantenimiento():
            return
        datos = self._leer_formulario()
        if datos is None:
            return
        codigo, nombre, precio, stock, lote, fecha, categoria = datos
        codigo = codigo or "AUTO"
        respuesta = ConexionC.ejecutar(
            "crear_prod", codigo, nombre, f"{precio:.2f}", f"{stock:.2f}",
            lote, fecha, categoria,
        )
        self._finalizar_crud(respuesta, "Producto creado correctamente.")

    def _on_editar(self) -> None:
        if self._sin_permiso_mantenimiento():
            return
        datos = self._leer_formulario()
        if datos is None:
            return
        codigo, nombre, precio, stock, lote, fecha, categoria = datos
        if not codigo:
            self._mostrar_estado(
                "Seleccione un producto o escriba el código a editar.", ok=False
            )
            return
        respuesta = ConexionC.ejecutar(
            "editar_prod", codigo, nombre, f"{precio:.2f}", f"{stock:.2f}",
            lote, fecha, categoria,
        )
        self._finalizar_crud(respuesta, "Producto actualizado correctamente.")

    def _on_eliminar(self) -> None:
        if self._sin_permiso_mantenimiento():
            return
        if not self.entry_codigo:
            return
        codigo = self.entry_codigo.get().strip()
        if not codigo:
            self._mostrar_estado("Seleccione o escriba el código a eliminar.", ok=False)
            return
        if not messagebox.askyesno(
            "Eliminar producto", f"¿Eliminar el producto {codigo}?"
        ):
            return

        respuesta = ConexionC.ejecutar("eliminar_prod", codigo)
        self._finalizar_crud(respuesta, "Producto eliminado correctamente.")

    def _finalizar_crud(self, respuesta: dict, mensaje_ok: str) -> None:
        if respuesta.get("status") == "OK":
            self._limpiar_formulario()
            self._on_buscar()
            self._mostrar_estado(mensaje_ok, ok=True)
        else:
            mensaje = respuesta.get("mensaje") or "No se pudo completar la operación."
            self._mostrar_estado(f"Error: {mensaje}", ok=False)

    def _sin_permiso_mantenimiento(self) -> bool:
        """Bloquea la escritura en inventario para roles distintos de ADMIN."""
        if self.puede_mantenimiento:
            return False
        self._mostrar_estado(
            "Permiso denegado: solo un usuario ADMIN puede modificar el inventario.",
            ok=False,
        )
        return True

    def _limpiar_formulario(self) -> None:
        for entrada in (
            self.entry_codigo, self.entry_nombre, self.entry_precio,
            self.entry_stock, self.entry_lote, self.entry_vencimiento,
        ):
            if entrada is not None:
                entrada.delete(0, "end")
        if self.combo_categoria is not None:
            self.combo_categoria.set(self.CATEGORIAS[0])

    @staticmethod
    def _poner_texto(entrada: ctk.CTkEntry | None, valor: object) -> None:
        if entrada is None:
            return
        entrada.delete(0, "end")
        entrada.insert(0, str(valor).strip())

    def _mostrar_estado(self, texto: str, ok: bool) -> None:
        self.lbl_estado.configure(
            text=texto, text_color="#2ea043" if ok else "#e5484d"
        )

    # -- Helpers --
    def _normalizar_fila(self, fila) -> tuple[str, ...]:
        """Normaliza [Código, Nombre, Precio, Stock, Vencimiento, Categoría]."""
        valores = [str(v).strip() for v in list(fila)]
        # Defensa: si el backend dejara un tag de sección al inicio.
        if valores and valores[0].upper() in {
            "PRODUCTO", "FILA", "STOCK", "VENCIMIENTO"
        }:
            valores = valores[1:]
        valores = valores[: len(self.COLUMNAS)]
        valores += [""] * (len(self.COLUMNAS) - len(valores))
        return tuple(valores)


# ===========================================================================
# VistaVentas (carrito de compras + cobro)
# ===========================================================================
class VistaVentas(ctk.CTkFrame):
    """Vista de ventas: agregar productos al carrito, ver total y procesar."""

    COLUMNAS = ("Código", "Nombre", "Precio Unit.", "Cantidad", "Subtotal")

    def __init__(self, master: ctk.CTk, rol: str = "VENDEDOR") -> None:
        super().__init__(master, fg_color="transparent")
        # Rol de la sesión: controla si se exige CMP al vender con receta.
        self.rol = (rol or "VENDEDOR").upper()
        # Último CMP ingresado; se envía al backend al procesar la venta.
        self._cmp_receta: str = ""
        self.grid_rowconfigure(0, weight=1)
        self.grid_columnconfigure(0, weight=1)

        # Zona superior (Agregar al carrito)
        top = ctk.CTkFrame(self, fg_color="transparent")
        top.grid(row=0, column=0, sticky="ew", padx=8, pady=(8, 4))
        top.grid_columnconfigure(0, weight=1)
        top.grid_columnconfigure(1, weight=0)

        # Combo editable: el usuario escribe y las sugerencias se llenan
        # dinámicamente desde el backend (sin listas hardcodeadas).
        self.entry_codigo = ctk.CTkComboBox(top, values=[], width=360)
        self.entry_codigo.grid(row=0, column=0, sticky="ew", padx=(0, 8))
        # CTkComboBox delega el teclado a su Entry interno. Se enlaza de forma
        # explícita para que CADA pulsación consulte al backend y actualice
        # las sugerencias (CTkComboBox.bind ya redirige, pero usamos el widget
        # interno para no depender de la versión de CustomTkinter).
        self._entrada_combo = getattr(
            self.entry_codigo, "_entry", self.entry_codigo
        )
        self._entrada_combo.bind(
            "<KeyRelease>", self._on_buscar_sugerencias, add="+"
        )
        self._entrada_combo.bind("<Return>", lambda _e: self._on_agregar())

        self.entry_cantidad = ctk.CTkEntry(
            top, placeholder_text="Cant. (ej. 0.5)", width=110
        )
        self.entry_cantidad.grid(row=0, column=1)
        self.btn_agregar = ctk.CTkButton(
            top, text="Agregar al Carrito", command=self._on_agregar
        )
        self.btn_agregar.grid(row=0, column=2)
        self.btn_quitar = ctk.CTkButton(
            top,
            text="Quitar seleccionado",
            width=150,
            fg_color="#e5484d",
            hover_color="#c93a3e",
            command=self._on_quitar_seleccionado,
        )
        self.btn_quitar.grid(row=0, column=3, padx=(8, 0))

        # Zona central (Carrito)
        frame_carrito = ctk.CTkFrame(
            self, fg_color=("gray75", "gray25"), corner_radius=12
        )
        frame_carrito.grid(row=1, column=0, sticky="nsew", padx=8, pady=8)
        frame_carrito.grid_rowconfigure(0, weight=1)
        frame_carrito.grid_columnconfigure(0, weight=1)

        self.tree = ttk.Treeview(frame_carrito, columns=self.COLUMNAS, show="headings")
        scrollbar = ttk.Scrollbar(frame_carrito, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=scrollbar.set)
        self.tree.grid(row=0, column=0, sticky="nsew")
        scrollbar.grid(row=0, column=1, padx=(0, 4), sticky="ns")
        for col in self.COLUMNAS:
            self.tree.heading(col, text=col, anchor="w")
            self.tree.column(col, width=100, anchor="w")
        self._configurar_estilo_treeview()

        # Quitar productos del carrito: clic derecho + tecla Delete.
        self._menu_carrito = tk.Menu(self, tearoff=0)
        self._menu_carrito.add_command(
            label="Quitar producto del carrito",
            command=self._on_quitar_seleccionado,
        )
        self.tree.bind("<Button-3>", self._on_click_derecho)
        # macOS usa Button-2 para el clic derecho.
        self.tree.bind("<Button-2>", self._on_click_derecho)
        self.tree.bind("<Delete>", lambda _e: self._on_quitar_seleccionado())

        # Zona inferior (Cobro)
        zona_cobro = ctk.CTkFrame(self, fg_color="transparent")
        zona_cobro.grid(row=2, column=0, sticky="ew", padx=8, pady=(0, 8))

        self.lbl_total = ctk.CTkLabel(
            zona_cobro, text="TOTAL: S/ 0.00", font=ctk.CTkFont(size=18, weight="bold")
        )
        self.lbl_total.grid(row=0, column=0, sticky="w")

        self.entry_dni = ctk.CTkEntry(zona_cobro, placeholder_text="DNI del cliente", width=140)
        self.entry_dni.grid(row=0, column=1, padx=16)

        self.lbl_error = ctk.CTkLabel(zona_cobro, text="", text_color="#e5484d")
        self.lbl_error.grid(row=0, column=2, padx=16)

        self.lbl_exito = ctk.CTkLabel(zona_cobro, text="", text_color="#2ea043")
        self.lbl_exito.grid(row=0, column=3, padx=16)

        self.btn_procesar = ctk.CTkButton(
            zona_cobro,
            text="Procesar Venta",
            fg_color="#2ea043",
            hover_color="#258a3d",
            command=self._on_procesar,
        )
        self.btn_procesar.grid(row=0, column=4)

        self.lbl_error.configure(text="")

    # -- Estilo Treeview (mismo tema oscuro que VistaInventario) --
    def _configurar_estilo_treeview(self) -> None:
        estilo = ttk.Style()
        estilo.theme_use("clam")
        estilo.configure(
            "Treeview",
            background="#343541",
            foreground="#DCE4EE",
            fieldbackground="#343541",
            rowheight=28,
            font=("Roboto", 12),
        )
        estilo.map(
            "Treeview",
            background=[("selected", "#4F4F4F")],
            foreground=[("selected", "#FFFFFF")],
        )
        estilo.configure(
            "Treeview.Heading",
            background="#343541",
            foreground="#DCE4EE",
            font=("Roboto", 11, "bold"),
            relief="flat",
        )
        estilo.map(
            "Treeview.Heading",
            background=[("active", "#3F3F4B")],
        )

    # -- Autocomplete: actualiza el desplegable con los nombres del backend --
    def _on_buscar_sugerencias(self, _event=None) -> None:
        termino = self.entry_codigo.get().strip()

        if not termino:
            self.entry_codigo.configure(values=[])
            return

        # Se consulta el backend en CADA pulsación para filtrar dinámicamente.
        respuesta = ConexionC.ejecutar("buscar_prod", termino)
        if respuesta.get("status") != "OK":
            self.entry_codigo.configure(values=[])
            return

        nombres: list[str] = []
        # Contrato buscar_prod: [[Código, Nombre, Precio, Stock, Vencimiento], ...]
        for fila in respuesta.get("data") or []:
            if len(fila) > 1:
                nombre = str(fila[1]).strip()
                if nombre and nombre not in nombres:
                    nombres.append(nombre)
            if len(nombres) >= 50:
                break

        self.entry_codigo.configure(values=nombres)

    # -- Zona superior: agregar al carrito --
    def _on_agregar(self) -> None:
        termino = self.entry_codigo.get().strip()
        if not termino:
            self.lbl_error.configure(text="Ingrese el nombre o código del producto.")
            return

        cantidad_txt = self.entry_cantidad.get().strip().replace(",", ".")
        try:
            cantidad = float(cantidad_txt or "1")
        except ValueError:
            self.lbl_error.configure(text="Cantidad inválida.")
            return
        if cantidad <= 0:
            self.lbl_error.configure(text="La cantidad debe ser mayor que cero.")
            return
        cantidad = round(cantidad, 2)

        # Búsqueda flexible: acepta nombre parcial o código (comando buscar_prod).
        respuesta = ConexionC.ejecutar("buscar_prod", termino)

        if respuesta.get("status") != "OK":
            mensaje = respuesta.get("mensaje") or "Producto no encontrado."
            self.lbl_error.configure(text=f"Error: {mensaje}")
            return

        data = respuesta.get("data") or []
        if not data:
            self.lbl_error.configure(
                text="Producto no encontrado. Verifique el código o nombre."
            )
            return
        # Contrato buscar_prod: [[Código, Nombre, Precio, Stock, Vencimiento], ...]
        fila = data[0] if len(data) > 0 else []
        codigo = str(fila[0]).strip() if len(fila) > 0 else termino
        nombre = str(fila[1]).strip() if len(fila) > 1 else termino
        try:
            precio = float(fila[2]) if len(fila) > 2 else 0.0
        except (ValueError, IndexError):
            precio = 0.0
        try:
            stock = float(fila[3]) if len(fila) > 3 else 0.0
        except (ValueError, IndexError):
            stock = 0.0
        # El backend añade el flag de receta como 7.ª columna (1/0).
        requiere_receta = False
        if len(fila) > 6:
            requiere_receta = str(fila[6]).strip().lower() in {
                "1", "t", "true", "si", "sí", "yes",
            }

        # Si el producto ya está en el carrito, se acumula la cantidad decimal.
        for item in self.tree.get_children():
            v = self.tree.item(item, "values")
            if str(v[0]).strip() != codigo:
                continue
            try:
                existente = float(str(v[3]).strip().replace(",", "."))
            except (ValueError, IndexError):
                existente = 0.0
            nueva_cantidad = round(existente + cantidad, 2)
            if nueva_cantidad > stock + 1e-9:
                self.lbl_error.configure(
                    text=f"Stock insuficiente: solo quedan {stock:g} unidades."
                )
                return
            # Control de recetas: bloquea si se cancela o el CMP está vacío.
            if not self._validar_receta(requiere_receta, nombre):
                return
            self.tree.item(
                item,
                values=(
                    codigo,
                    nombre,
                    f"{precio:.2f}",
                    self._formatear_cantidad(nueva_cantidad),
                    f"{round(precio * nueva_cantidad, 2):.2f}",
                ),
            )
            self.lbl_error.configure(text="")
            self.entry_cantidad.delete(0, "end")
            self._actualizar_total()
            return

        if cantidad > stock + 1e-9:
            self.lbl_error.configure(
                text=f"Stock insuficiente: solo quedan {stock:g} unidades."
            )
            return

        # Control de recetas: bloquea si se cancela o el CMP está vacío.
        if not self._validar_receta(requiere_receta, nombre):
            return

        subtotal = round(precio * cantidad, 2)
        self.tree.insert(
            "",
            "end",
            values=(
                codigo,
                nombre,
                f"{precio:.2f}",
                self._formatear_cantidad(cantidad),
                f"{subtotal:.2f}",
            ),
        )
        self.lbl_error.configure(text="")
        self.entry_cantidad.delete(0, "end")
        self._actualizar_total()

    def _validar_receta(self, requiere_receta: bool, nombre: str) -> bool:
        """Exige el CMP para productos con receta si el rol no es ADMIN.

        Devuelve True si la inserción puede continuar (no requiere receta, el
        rol está exento o se ingresó un CMP) y False si se canceló el diálogo
        o el CMP quedó vacío (bloquea la inserción)."""
        if not requiere_receta or self.rol not in {"VENDEDOR", "TECNICO"}:
            return True

        dialogo = ctk.CTkInputDialog(
            text=(
                f"El producto '{nombre}' requiere receta médica.\n"
                "Ingrese el Número de Colegiatura Médica (CMP):"
            ),
            title="Receta requerida",
        )
        cmp = (dialogo.get_input() or "").strip()
        if not cmp:
            self.lbl_error.configure(
                text="No se agregó: receta cancelada o CMP vacío."
            )
            # Bitácora de seguridad: la venta se bloquea por cancelar la receta.
            ConexionC.ejecutar(
                "log_seguridad",
                f"Venta bloqueada: '{nombre}' requiere receta y se "
                f"canceló u omitió el CMP (rol {self.rol})",
            )
            return False

        self._cmp_receta = cmp
        self.lbl_error.configure(text="")
        return True

    # -- Quitar productos del carrito --
    def _on_click_derecho(self, event) -> None:
        fila = self.tree.identify_row(event.y)
        if not fila:
            return
        self.tree.selection_set(fila)
        self.tree.focus(fila)
        try:
            self._menu_carrito.tk_popup(event.x_root, event.y_root)
        finally:
            self._menu_carrito.grab_release()

    def _on_quitar_seleccionado(self) -> None:
        seleccion = self.tree.selection()
        if not seleccion:
            self.lbl_error.configure(
                text="Seleccione un producto del carrito para quitarlo."
            )
            return
        for item in seleccion:
            self.tree.delete(item)
        self.lbl_error.configure(text="")
        self._actualizar_total()

    def _actualizar_total(self) -> None:
        self.lbl_total.configure(text=f"TOTAL: S/ {self._calcular_total():.2f}")

    @staticmethod
    def _formatear_cantidad(cantidad: float) -> str:
        """Muestra 0.50 como 0.5 y 1.00 como 1, sin perder fracciones."""
        texto = f"{cantidad:.2f}".rstrip("0").rstrip(".")
        return texto or "0"

    # -- Zona inferior: procesar venta --
    def _on_procesar(self) -> None:
        self.lbl_error.configure(text="")
        self.lbl_exito.configure(text="")

        filas: list[tuple[str, ...]] = []
        items: list[str] = []
        for item in self.tree.get_children():
            v = self.tree.item(item, "values")
            try:
                codigo = str(v[0]).strip()
                cantidad = float(str(v[3]).strip().replace(",", "."))
                cantidad = round(cantidad, 2)
            except (ValueError, IndexError):
                continue
            if cantidad <= 0:
                continue
            filas.append(tuple(str(valor) for valor in v))
            # Se envía con dos decimales para preservar fracciones (0.50).
            items.append(f"{codigo},{cantidad:.2f}")

        if not items:
            self.lbl_error.configure(text="El carrito está vacío.")
            return

        total_venta = self._calcular_total()
        carrito = "|".join(items)
        dni = self.entry_dni.get().strip()
        if dni and (len(dni) != 8 or not dni.isdigit()):
            self.lbl_error.configure(
                text="El DNI debe tener 8 dígitos (o dejarse vacío)."
            )
            return

        # El CMP viaja al backend junto con el carrito (puede ir vacío).
        respuesta = ConexionC.ejecutar(
            "procesar_venta", dni, carrito, self._cmp_receta
        )

        if respuesta.get("status") == "OK":
            # Respuesta del backend: OK;VENTA;<id>;<total>
            # Se genera físicamente boleta_<id>.txt con fecha, DNI y detalles.
            id_venta = self._extraer_id_venta(respuesta)
            total_backend = self._extraer_total_venta(respuesta)
            total_boleta = total_backend if total_backend is not None else total_venta
            ruta_boleta = self._generar_boleta(id_venta, filas, dni, total_boleta)
            self.tree.delete(*self.tree.get_children())
            self.lbl_total.configure(text="TOTAL: S/ 0.00")
            self._cmp_receta = ""
            self.lbl_exito.configure(
                text=f"Venta exitosa. Boleta #{id_venta} impresa ({ruta_boleta.name})."
            )
            self.lbl_error.configure(text="")
        else:
            mensaje = respuesta.get("mensaje") or "No se pudo procesar la venta."
            self.lbl_error.configure(text=f"Error: {mensaje}")

    @staticmethod
    def _extraer_id_venta(respuesta: dict) -> str:
        """Obtiene el ID de la fila OK;VENTA;<id>;<total> del backend.

        Soporta tanto la respuesta cruda con tag (["VENTA", id, total])
        como la respuesta ya normalizada por ConexionC (["id", total]).
        """
        for fila in respuesta.get("data") or []:
            valores = [str(v).strip() for v in fila]
            if not valores:
                continue
            if valores[0].upper() == "VENTA":
                valores = valores[1:]
            if valores and valores[0].isdigit():
                return valores[0]
        # Respaldo: identificador único basado en la fecha/hora actual.
        return datetime.datetime.now().strftime("%Y%m%d%H%M%S")

    @staticmethod
    def _extraer_total_venta(respuesta: dict) -> float | None:
        """Obtiene el total de OK;VENTA;<id>;<total> si el backend lo envía."""
        for fila in respuesta.get("data") or []:
            valores = [str(v).strip() for v in fila]
            if not valores:
                continue
            if valores[0].upper() == "VENTA":
                valores = valores[1:]
            if len(valores) > 1:
                try:
                    return float(valores[1])
                except ValueError:
                    return None
        return None

    def _generar_boleta(
        self,
        id_venta: str,
        filas: list[tuple[str, ...]],
        dni: str,
        total: float,
    ) -> Path:
        """Escribe el resumen de la compra en boleta_<id>.txt (texto plano)."""
        ruta = Path(__file__).resolve().parent / f"boleta_{id_venta}.txt"
        fecha = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        lineas = [
            "=" * 48,
            "            FARMACIA URBANA",
            "         BOLETA DE VENTA ELECTRONICA",
            "=" * 48,
            f"Boleta N°: {id_venta}",
            f"Fecha: {fecha}",
            f"DNI cliente: {dni or '-'}",
            "-" * 48,
            f"{'Codigo':<12}{'Producto':<20}{'Cant.':>6}{'Subtotal':>10}",
            "-" * 48,
        ]
        for fila in filas:
            codigo = fila[0] if len(fila) > 0 else ""
            nombre = fila[1] if len(fila) > 1 else ""
            cantidad = fila[3] if len(fila) > 3 else ""
            subtotal = fila[4] if len(fila) > 4 else "0.00"
            lineas.append(f"{codigo:<12}{nombre[:19]:<20}{cantidad:>6}{subtotal:>10}")

        lineas += [
            "-" * 48,
            f"TOTAL: S/ {total:.2f}",
            "=" * 48,
            "Gracias por su compra.",
            "",
        ]
        ruta.write_text("\n".join(lineas), encoding="utf-8")
        return ruta

    # -- Helpers --
    def _calcular_total(self) -> float:
        total = 0.0
        for item in self.tree.get_children():
            v = self.tree.item(item, "values")
            try:
                total += float(v[4])
            except (ValueError, IndexError):
                continue
        return total


# ===========================================================================
# VistaReportes (historial de ventas por rango de fechas)
# ===========================================================================
class VistaReportes(ctk.CTkFrame):
    """Vista de reportes: historial de ventas filtrado por fechas."""

    COLUMNAS = ("ID Venta", "DNI Cliente", "Fecha y Hora Exacta", "Total")
    ANCHOS = {
        "ID Venta": 90,
        "DNI Cliente": 130,
        "Fecha y Hora Exacta": 220,
        "Total": 100,
    }

    def __init__(self, master: ctk.CTk) -> None:
        super().__init__(master, fg_color="transparent")
        self.grid_rowconfigure(0, weight=0)
        self.grid_rowconfigure(1, weight=1)
        self.grid_rowconfigure(2, weight=0)
        self.grid_rowconfigure(3, weight=0)
        self.grid_rowconfigure(4, weight=0)
        self.grid_columnconfigure(0, weight=1)

        # Zona superior: filtros de fecha + boton generar
        top = ctk.CTkFrame(self, fg_color="transparent")
        top.grid(row=0, column=0, sticky="ew", padx=8, pady=(8, 4))
        top.grid_columnconfigure(0, weight=1)
        top.grid_columnconfigure(1, weight=0)

        self.entry_fecha_inicio = ctk.CTkEntry(
            top, placeholder_text="Fecha Inicio (YYYY-MM-DD)"
        )
        self.entry_fecha_inicio.grid(row=0, column=0, sticky="ew", padx=(0, 8))
        self.entry_fecha_fin = ctk.CTkEntry(
            top, placeholder_text="Fecha Fin (YYYY-MM-DD)"
        )
        self.entry_fecha_fin.grid(row=0, column=1, sticky="e")

        # Por defecto: primer y último día del mes actual.
        hoy = datetime.date.today()
        primer_dia = hoy.replace(day=1)
        siguiente_mes = (
            primer_dia.replace(year=primer_dia.year + 1, month=1)
            if primer_dia.month == 12
            else primer_dia.replace(month=primer_dia.month + 1)
        )
        ultimo_dia = siguiente_mes - datetime.timedelta(days=1)
        self.entry_fecha_inicio.insert(0, primer_dia.isoformat())
        self.entry_fecha_fin.insert(0, ultimo_dia.isoformat())

        self.btn_generar = ctk.CTkButton(
            top, text="Generar Historial", command=self._on_generar
        )
        self.btn_generar.grid(row=0, column=2)

        # Zona central: tabla historial
        frame_tabla = ctk.CTkFrame(
            self, fg_color=("gray75", "gray25"), corner_radius=12
        )
        frame_tabla.grid(row=1, column=0, sticky="nsew", padx=8, pady=8)
        frame_tabla.grid_rowconfigure(0, weight=1)
        frame_tabla.grid_columnconfigure(0, weight=1)

        self.tree = ttk.Treeview(frame_tabla, columns=self.COLUMNAS, show="headings")
        scrollbar = ttk.Scrollbar(frame_tabla, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=scrollbar.set)
        self.tree.grid(row=0, column=0, sticky="nsew")
        scrollbar.grid(row=0, column=1, padx=(0, 4), sticky="ns")
        for col in self.COLUMNAS:
            self.tree.heading(col, text=col, anchor="w")
            self.tree.column(col, width=self.ANCHOS[col], anchor="w")
        self._configurar_estilo_treeview()

        # Zona inferior: estado
        self.lbl_error = ctk.CTkLabel(
            self, text="", font=ctk.CTkFont(size=13), text_color="#e5484d"
        )
        self.lbl_error.grid(row=2, column=0, sticky="ew", padx=8, pady=(0, 6))

        self.lbl_producto_top = ctk.CTkLabel(
            self, text="Producto Más Vendido: -", font=ctk.CTkFont(size=13)
        )
        self.lbl_producto_top.grid(row=3, column=0, sticky="ew", padx=8, pady=(0, 4))

        self.lbl_ingresos = ctk.CTkLabel(
            self,
            text="Ingresos Totales: S/ 0.00",
            font=ctk.CTkFont(size=13, weight="bold"),
        )
        self.lbl_ingresos.grid(row=4, column=0, sticky="ew", padx=8, pady=(0, 6))

    # -- Estilo Treeview (mismo tema oscuro que el resto) --
    def _configurar_estilo_treeview(self) -> None:
        estilo = ttk.Style()
        estilo.theme_use("clam")
        estilo.configure(
            "Treeview",
            background="#343541",
            foreground="#DCE4EE",
            fieldbackground="#343541",
            rowheight=28,
            font=("Roboto", 12),
        )
        estilo.map(
            "Treeview",
            background=[("selected", "#4F4F4F")],
            foreground=[("selected", "#FFFFFF")],
        )
        estilo.configure(
            "Treeview.Heading",
            background="#343541",
            foreground="#DCE4EE",
            font=("Roboto", 11, "bold"),
            relief="flat",
        )
        estilo.map(
            "Treeview.Heading",
            background=[("active", "#3F3F4B")],
        )

    # -- Accion del boton Generar Historial --
    def _on_generar(self) -> None:
        fecha_inicio = self.entry_fecha_inicio.get().strip()
        fecha_fin = self.entry_fecha_fin.get().strip()
        self.lbl_error.configure(text="")

        respuesta = ConexionC.ejecutar("historial", fecha_inicio, fecha_fin)

        if respuesta.get("status") == "OK":
            datos = respuesta.get("data") or []
            self.tree.delete(*self.tree.get_children())
            for fila in datos:
                self.tree.insert("", "end", values=self._normalizar_fila(fila))
        else:
            self.tree.delete(*self.tree.get_children())
            self.lbl_error.configure(
                text=f"Error: {respuesta.get('mensaje') or 'No se generó el historial.'}"
            )

        # Las métricas también respetan el reporte abierto (fechas vacías).
        self._actualizar_metricas(fecha_inicio, fecha_fin)

    def _actualizar_metricas(self, fecha_inicio: str, fecha_fin: str) -> None:
        """Actualiza producto más vendido e ingresos del mismo periodo."""
        resp_top = ConexionC.ejecutar(
            "producto_mas_vendido", fecha_inicio, fecha_fin
        )
        texto_top = "Producto Más Vendido: -"
        if resp_top.get("status") == "OK" and resp_top.get("data"):
            fila = [str(v).strip() for v in list(resp_top["data"][0])]
            if fila and fila[0].upper() == "PRODUCTO_MAS_VENDIDO":
                fila = fila[1:]
            if len(fila) >= 2:
                nombre = fila[1]
                cantidad = fila[2] if len(fila) > 2 else "0"
                texto_top = f"Producto Más Vendido: {nombre} ({cantidad} unidades)"
        self.lbl_producto_top.configure(text=texto_top)

        resp_ing = ConexionC.ejecutar(
            "ingresos_totales", fecha_inicio, fecha_fin
        )
        total = 0.0
        if resp_ing.get("status") == "OK" and resp_ing.get("data"):
            fila = [str(v).strip() for v in list(resp_ing["data"][0])]
            if fila and fila[0].upper() == "INGRESOS_TOTALES":
                fila = fila[1:]
            if fila:
                try:
                    total = float(fila[0])
                except ValueError:
                    total = 0.0
        self.lbl_ingresos.configure(text=f"Ingresos Totales: S/ {total:.2f}")

    # -- Helpers --
    def _normalizar_fila(self, fila) -> tuple[str, ...]:
        valores = [str(v).strip() for v in list(fila)[: len(self.COLUMNAS)]]
        valores += [""] * (len(self.COLUMNAS) - len(valores))
        return tuple(valores)


# ===========================================================================
# VistaAlertas (alertas de stock y proximos a vencer)
# ===========================================================================
class VistaAlertas(ctk.CTkFrame):
    """Vista de alertas: stock critico y proximos a vencer."""

    def __init__(self, master: ctk.CTk) -> None:
        super().__init__(master, fg_color="transparent")
        self.grid_rowconfigure(0, weight=0)
        self.grid_rowconfigure(1, weight=1)
        self.grid_rowconfigure(2, weight=1)
        self.grid_rowconfigure(3, weight=0)
        self.grid_columnconfigure(0, weight=1)

        self.btn_refrescar = ctk.CTkButton(
            self, text="Refrescar Alertas", command=self._on_refrescar
        )
        self.btn_refrescar.grid(row=0, column=0, sticky="e", padx=8, pady=(8, 0))

        frame_stock = ctk.CTkFrame(
            self, fg_color=("gray75", "gray25"), corner_radius=12
        )
        frame_stock.grid(row=1, column=0, sticky="nsew", padx=8, pady=8)
        frame_stock.grid_rowconfigure(0, weight=1)
        frame_stock.grid_columnconfigure(0, weight=1)
        self.tree_stock = ttk.Treeview(
            frame_stock, columns=("Código", "Nombre", "Stock"), show="headings"
        )
        scroll_stock = ttk.Scrollbar(
            frame_stock, orient="vertical", command=self.tree_stock.yview
        )
        self.tree_stock.configure(yscrollcommand=scroll_stock.set)
        self.tree_stock.grid(row=0, column=0, sticky="nsew")
        scroll_stock.grid(row=0, column=1, padx=(0, 4), sticky="ns")
        for col, ancho in (("Código", 80), ("Nombre", 180), ("Stock", 70)):
            self.tree_stock.heading(col, text=col, anchor="w")
            self.tree_stock.column(col, width=ancho, anchor="w")

        frame_venc = ctk.CTkFrame(
            self, fg_color=("gray75", "gray25"), corner_radius=12
        )
        frame_venc.grid(row=2, column=0, sticky="nsew", padx=8, pady=(0, 8))
        frame_venc.grid_rowconfigure(0, weight=1)
        frame_venc.grid_columnconfigure(0, weight=1)
        self.tree_venc = ttk.Treeview(
            frame_venc, columns=("Código", "Nombre", "Vencimiento"), show="headings"
        )
        scroll_venc = ttk.Scrollbar(
            frame_venc, orient="vertical", command=self.tree_venc.yview
        )
        self.tree_venc.configure(yscrollcommand=scroll_venc.set)
        self.tree_venc.grid(row=0, column=0, sticky="nsew")
        scroll_venc.grid(row=0, column=1, padx=(0, 4), sticky="ns")
        for col, ancho in (("Código", 80), ("Nombre", 180), ("Vencimiento", 110)):
            self.tree_venc.heading(col, text=col, anchor="w")
            self.tree_venc.column(col, width=ancho, anchor="w")

        self.lbl_error = ctk.CTkLabel(
            self, text="", font=ctk.CTkFont(size=13), text_color="#e5484d"
        )
        self.lbl_error.grid(row=3, column=0, sticky="ew", padx=8, pady=(0, 6))

        # Carga inicial de ambas tablas al abrir la vista.
        self._on_refrescar()

    @staticmethod
    def _filas_alerta(respuesta: dict, etiqueta: str) -> list[tuple[str, ...]]:
        """Normaliza las filas de una alerta y elimina el tag de sección si viene."""
        filas: list[tuple[str, ...]] = []
        for fila in respuesta.get("data") or []:
            valores = [str(v).strip() for v in list(fila)]
            if valores and valores[0].upper() == etiqueta:
                valores = valores[1:]
            valores = valores[:3]
            valores += [""] * (3 - len(valores))
            filas.append(tuple(valores))
        return filas

    def _on_refrescar(self) -> None:
        errores: list[str] = []

        resp_stock = ConexionC.ejecutar("alertas_stock")
        self.tree_stock.delete(*self.tree_stock.get_children())
        if resp_stock.get("status") == "OK":
            for fila in self._filas_alerta(resp_stock, "STOCK"):
                self.tree_stock.insert("", "end", values=fila)
        else:
            errores.append(
                "stock: " + str(resp_stock.get("mensaje") or "error desconocido")
            )

        resp_venc = ConexionC.ejecutar("alertas_vencimientos")
        self.tree_venc.delete(*self.tree_venc.get_children())
        if resp_venc.get("status") == "OK":
            for fila in self._filas_alerta(resp_venc, "VENCIMIENTO"):
                self.tree_venc.insert("", "end", values=fila)
        else:
            errores.append(
                "vencimientos: " + str(resp_venc.get("mensaje") or "error desconocido")
            )

        if errores:
            self.lbl_error.configure(text="Error al cargar alertas -> " + "; ".join(errores))
        else:
            self.lbl_error.configure(text="")


# ===========================================================================
# DashboardFrame (sidebar + area central)
# ===========================================================================
class DashboardFrame(ctk.CTkFrame):
    """Panel principal tras un login exitoso."""

    MODULOS = ("Ventas", "Inventario", "Reportes", "Alertas", "Usuarios")

    def __init__(
        self,
        master: ctk.CTk,
        usuario: str,
        on_logout: Callable[[], None],
        rol: str = "VENDEDOR",
        id_sucursal: int = 1,
        nombre_sucursal: str = "Sede Central",
    ) -> None:
        super().__init__(master, fg_color="transparent")
        self._on_logout_cb = on_logout
        self._vista_actual: ctk.CTkFrame | None = None
        self._modulo_actual: str | None = None
        self.usuario = usuario
        self.rol = (rol or "VENDEDOR").upper()
        self.es_admin = self.rol.startswith("ADMIN")
        self.id_sucursal = id_sucursal
        self.nombre_sucursal = nombre_sucursal
        self._sucursales: dict[str, int] = {}

        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(0, weight=1)

        # ---- Sidebar izquierda ----
        sidebar = ctk.CTkFrame(self, width=220, corner_radius=0)
        sidebar.grid(row=0, column=0, sticky="nsew")
        # Filas 0 y 1: título y datos del usuario (cada uno en su propia fila
        # para que no se superpongan). Filas 2-6: botones de módulos.
        for fila_sidebar in range(9):
            sidebar.grid_rowconfigure(fila_sidebar, weight=0)
        sidebar.grid_rowconfigure(7, weight=1)  # resorte antes del cierre

        # Encabezado con aire suficiente para que el título y los datos del
        # usuario nunca se monten sobre el primer botón de navegación.
        ctk.CTkLabel(
            sidebar, text="Farmacia Urbana", font=ctk.CTkFont(size=18, weight="bold")
        ).grid(row=0, column=0, padx=20, pady=(36, 10))
        self.lbl_usuario_sucursal = ctk.CTkLabel(
            sidebar,
            text=f"Usuario: {usuario}\nSucursal: {nombre_sucursal}",
            justify="center",
        )
        self.lbl_usuario_sucursal.grid(row=1, column=0, padx=20, pady=(0, 30))

        self._botones: dict[str, ctk.CTkButton] = {}
        visibles = self._modulos_visibles()
        fila = 2
        for modulo in self.MODULOS:
            if modulo not in visibles:
                continue
            btn = ctk.CTkButton(
                sidebar,
                text=modulo,
                anchor="w",
                fg_color="transparent",
                text_color=("gray10", "gray90"),
                hover_color=("gray70", "gray30"),
                command=lambda m=modulo: self.mostrar_vista(m),
            )
            btn.grid(row=fila, column=0, padx=16, pady=(4, 6), sticky="ew")
            self._botones[modulo] = btn
            fila += 1

        # Resorte: CTkFrame transparente en fila 7 con weight=1 -> empuja el cierre al fondo
        resorte = ctk.CTkFrame(sidebar, fg_color="transparent", corner_radius=0)
        resorte.grid(row=7, column=0, sticky="nsew")

        ctk.CTkButton(
            sidebar,
            text="Cerrar sesion",
            fg_color="#e5484d",
            hover_color="#c93a3e",
            command=self._cerrar_sesion,
        ).grid(row=8, column=0, padx=16, pady=20, sticky="ew")

        # ---- Area central dinamica ----
        self.contenedor = ctk.CTkFrame(
            self, fg_color="transparent", corner_radius=12
        )
        self.contenedor.grid(
            row=0, column=1, sticky="nsew", padx=16, pady=16
        )
        self.contenedor.grid_columnconfigure(0, weight=1)
        self.contenedor.grid_rowconfigure(0, weight=1)

        # ---- Selector de sucursal (solo ADMIN, esquina superior derecha) ----
        self.option_sucursal_admin: ctk.CTkOptionMenu | None = None
        if self.es_admin:
            self._sucursales = self._cargar_sucursales()
            if self.nombre_sucursal not in self._sucursales:
                self._sucursales[self.nombre_sucursal] = self.id_sucursal

            self.option_sucursal_admin = ctk.CTkOptionMenu(
                self,
                values=list(self._sucursales.keys()),
                width=190,
                command=self._cambiar_sucursal_admin,
            )
            self.option_sucursal_admin.set(self.nombre_sucursal)
            self.option_sucursal_admin.place(
                relx=1.0, rely=0.0, anchor="ne", x=-16, y=16
            )

        # ---- Boton Chatbot en la esquina superior derecha ----
        self.btn_chatbot = ctk.CTkButton(
            self, text="💬 Chatbot", command=self._abrir_chatbot
        )
        # Se desplaza a la izquierda del selector para no superponerse.
        self.btn_chatbot.place(
            relx=1.0, rely=0.0, anchor="ne",
            x=-220 if self.es_admin else -16, y=16,
        )

        self.mostrar_vista(self.MODULOS[0])

    # -- Jerarquía de roles: módulos visibles en el menú lateral --
    def _modulos_visibles(self) -> tuple[str, ...]:
        """Define el menú lateral según el rol.

        ADMIN    -> todos los módulos.
        TECNICO  -> Ventas, Inventario y Alertas (sin Reportes ni Usuarios).
        VENDEDOR -> Ventas, Inventario y Alertas (Inventario en modo lectura).
        El Chatbot es una ventana aparte y está disponible para todos.
        """
        if self.es_admin:
            return self.MODULOS
        # TECNICO y VENDEDOR comparten el mismo menú; el inventario se abre en
        # modo lectura porque VistaInventario solo habilita escritura a ADMIN.
        return ("Ventas", "Inventario", "Alertas")

    # -- Sucursales (auditoría del ADMIN) --
    def _cargar_sucursales(self) -> dict[str, int]:
        """Carga el catálogo de sucursales desde el backend C."""
        respuesta = ConexionC.ejecutar("listar_sucursales")
        mapa: dict[str, int] = {}
        if respuesta.get("status") == "OK":
            for fila in respuesta.get("data") or []:
                valores = [str(v).strip() for v in list(fila)]
                if len(valores) >= 2 and valores[0].isdigit():
                    mapa[valores[1]] = int(valores[0])

        # Respaldo para instalaciones antiguas sin tabla `sucursal`.
        if not mapa:
            mapa = {"Sede Central": 1, "Sede Norte": 2}
        return mapa

    def _cambiar_sucursal_admin(self, nombre_sucursal: str) -> None:
        """Cambia la sucursal activa y refresca la vista para auditarla."""
        id_sucursal = self._sucursales.get(nombre_sucursal)
        if id_sucursal is None:
            return

        self.id_sucursal = id_sucursal
        self.nombre_sucursal = nombre_sucursal
        ConexionC.establecer_sucursal(id_sucursal)
        os.environ["FARMACIA_SUCURSAL"] = str(id_sucursal)

        # Mantener sincronizado el estado del controlador raíz: si el ADMIN
        # cambia de sede, la sesión completa (y un futuro logout/login) ve la
        # misma sucursal que la vista que está auditando.
        raiz = self.winfo_toplevel()
        if hasattr(raiz, "id_sucursal_actual"):
            raiz.id_sucursal_actual = id_sucursal
        if hasattr(raiz, "nombre_sucursal_actual"):
            raiz.nombre_sucursal_actual = nombre_sucursal

        if self.option_sucursal_admin is not None:
            self.option_sucursal_admin.set(nombre_sucursal)

        self.lbl_usuario_sucursal.configure(
            text=f"Usuario: {self.usuario}\nSucursal: {nombre_sucursal}"
        )
        self.winfo_toplevel().title(f"Farmacia Urbana - {nombre_sucursal}")
        self._refrescar_vista_actual()

    def _refrescar_vista_actual(self) -> None:
        """Vuelve a construir la vista abierta con la sucursal seleccionada."""
        if self._modulo_actual is None:
            return
        modulo = self._modulo_actual
        self.mostrar_vista(modulo)
        # Reportes no consulta al abrirse: se fuerza el refresco del periodo.
        if isinstance(self._vista_actual, VistaReportes):
            self._vista_actual._on_generar()

    # -- Navegacion --
    def mostrar_vista(self, modulo: str) -> None:
        """Destruye la vista actual e instancia el módulo solicitado."""
        # Defensa: un rol no puede abrir módulos fuera de su jerarquía.
        if modulo not in self._modulos_visibles():
            return
        self._modulo_actual = modulo
        if self._vista_actual is not None:
            self._vista_actual.destroy()
            self._vista_actual = None

        if modulo == "Inventario":
            frame = VistaInventario(self.contenedor, rol=self.rol)
        elif modulo == "Ventas":
            frame = VistaVentas(self.contenedor, rol=self.rol)
        elif modulo == "Reportes":
            frame = VistaReportes(self.contenedor)
        elif modulo == "Alertas":
            frame = VistaAlertas(self.contenedor)
        elif modulo == "Usuarios":
            frame = VistaUsuarios(
                self.contenedor, id_sucursal_actual=self.id_sucursal
            )
        else:
            frame = ctk.CTkFrame(self.contenedor, fg_color="transparent")

        frame.grid(row=0, column=0, sticky="nsew")
        frame.grid_rowconfigure(0, weight=1)
        frame.grid_columnconfigure(0, weight=1)

        if modulo not in ("Inventario", "Ventas", "Reportes", "Alertas", "Usuarios"):
            ctk.CTkLabel(
                frame,
                text=f"Modulo: {modulo}",
                font=ctk.CTkFont(size=22, weight="bold"),
            ).grid(row=0, column=0)

        self._vista_actual = frame

    # -- Chatbot (ventana emergente) --
    def _abrir_chatbot(self) -> None:
        if getattr(self, "_ventana_chat", None) is not None and self._ventana_chat.winfo_exists():
            return
        self._ventana_chat = VentanaChatbot(self)

    def _cerrar_sesion(self) -> None:
        """Cierra la sesion y regresa a la pantalla de login."""
        self._on_logout_cb()


# ===========================================================================
# VentanaChatbot (asistente virtual - ventana emergente)
# ===========================================================================
class VentanaChatbot(ctk.CTkToplevel):
    """Asistente virtual Urbano: conversación con burbujas estilo mensajería."""

    COLOR_FONDO = "#0f1115"
    COLOR_BARRA = "#1c2026"
    COLOR_CHAT = "#15181c"
    COLOR_BURBUJA_BOT = "#262b33"
    COLOR_BURBUJA_USUARIO = "#2563eb"
    COLOR_TEXTO = "#e6edf3"
    COLOR_TEXTO_SUAVE = "#8b949e"
    COLOR_ACENTO = "#3b82f6"
    COLOR_EXITO = "#2ea043"

    PREGUNTAS_RAPIDAS = (
        "¿Cómo hago una venta?",
        "¿Qué es el CMP?",
        "¿Quién es el administrador?",
        "¿Cómo veo los reportes?",
        "stock de paracetamol",
        "ayuda",
    )

    # Mensajes técnicos del backend traducidos a lenguaje de usuario.
    ERRORES_AMIGABLES = {
        "conexion_bd_fallida": (
            "No pude conectarme con el inventario. Verifica que PostgreSQL "
            "esté disponible e intenta de nuevo."
        ),
        "consulta_bd_fallida": (
            "No pude completar la consulta al inventario. Intenta nuevamente "
            "en unos segundos."
        ),
        "FARMACIA_DB_CONN_no_definida": (
            "Falta la configuración de la base de datos (FARMACIA_DB_CONN). "
            "Avisa al administrador del sistema."
        ),
        "Sucursal_no_inicializada": (
            "Tu sesión no tiene una sucursal asignada. Vuelve a iniciar sesión."
        ),
    }

    def __init__(self, master: ctk.CTk) -> None:
        super().__init__(master)
        self.title("Urbano · Asistente Virtual")
        self.geometry("560x700")
        self.minsize(440, 560)
        self.configure(fg_color=self.COLOR_FONDO)
        self.grid_rowconfigure(1, weight=1)
        self.grid_columnconfigure(0, weight=1)

        self._construir_cabecera()
        self._construir_chat()
        self._construir_sugerencias()
        self._construir_entrada()

        self.protocol("WM_DELETE_WINDOW", self._cerrar)
        self.after(150, self._enfocar_entrada)

        # Mensaje de bienvenida del asistente.
        self._mostrar_mensaje(
            "¡Hola! Soy Urbano, el asistente virtual de Farmacia Urbana. "
            "Puedo consultar el stock y el vencimiento de tus medicamentos y "
            "guiarte en el uso del sistema. ¿En qué te ayudo hoy?",
            autor="bot",
        )

    # -- Construcción de la ventana -------------------------------------
    def _construir_cabecera(self) -> None:
        cabecera = ctk.CTkFrame(self, fg_color=self.COLOR_BARRA, corner_radius=0)
        cabecera.grid(row=0, column=0, sticky="ew")
        cabecera.grid_columnconfigure(1, weight=1)

        ctk.CTkLabel(
            cabecera,
            text="U",
            width=42,
            height=42,
            corner_radius=21,
            fg_color=self.COLOR_ACENTO,
            text_color="#ffffff",
            font=ctk.CTkFont(size=18, weight="bold"),
        ).grid(row=0, column=0, rowspan=2, padx=(16, 10), pady=12)

        ctk.CTkLabel(
            cabecera,
            text="Urbano",
            font=ctk.CTkFont(size=16, weight="bold"),
            anchor="w",
        ).grid(row=0, column=1, sticky="sw", pady=(12, 0))

        ctk.CTkLabel(
            cabecera,
            text="Asistente virtual · Farmacia Urbana",
            font=ctk.CTkFont(size=11),
            text_color=self.COLOR_TEXTO_SUAVE,
            anchor="w",
        ).grid(row=1, column=1, sticky="nw", pady=(0, 12))

        ctk.CTkLabel(
            cabecera,
            text="● En línea",
            font=ctk.CTkFont(size=11, weight="bold"),
            text_color=self.COLOR_EXITO,
        ).grid(row=0, column=2, rowspan=2, padx=16, sticky="e")

    def _construir_chat(self) -> None:
        self.chat_box = ctk.CTkScrollableFrame(
            self, fg_color=self.COLOR_CHAT, corner_radius=14
        )
        self.chat_box.grid(row=1, column=0, sticky="nsew", padx=12, pady=10)
        self.chat_box.grid_columnconfigure(0, weight=1)

    def _construir_sugerencias(self) -> None:
        panel = ctk.CTkFrame(self, fg_color="transparent")
        panel.grid(row=2, column=0, sticky="ew", padx=12, pady=(0, 6))
        for columna in range(3):
            panel.grid_columnconfigure(columna, weight=1)

        ctk.CTkLabel(
            panel,
            text="Preguntas frecuentes",
            font=ctk.CTkFont(size=10, weight="bold"),
            text_color=self.COLOR_TEXTO_SUAVE,
            anchor="w",
        ).grid(row=0, column=0, columnspan=3, sticky="w", padx=2, pady=(0, 4))

        for indice, pregunta in enumerate(self.PREGUNTAS_RAPIDAS):
            fila = 1 + indice // 3
            columna = indice % 3
            ctk.CTkButton(
                panel,
                text=pregunta,
                command=lambda p=pregunta: self._enviar(p),
                height=28,
                corner_radius=14,
                border_width=1,
                border_color="#30363d",
                fg_color="#1f242a",
                hover_color="#2b323b",
                text_color="#c9d1d9",
                font=ctk.CTkFont(size=11),
            ).grid(row=fila, column=columna, sticky="ew", padx=2, pady=2)

    def _construir_entrada(self) -> None:
        pie = ctk.CTkFrame(self, fg_color=self.COLOR_BARRA, corner_radius=0)
        pie.grid(row=3, column=0, sticky="ew")
        pie.grid_columnconfigure(0, weight=1)

        self.chat_entry = ctk.CTkEntry(
            pie,
            placeholder_text="Escribe tu mensaje a Urbano...",
            height=42,
            corner_radius=21,
            border_width=0,
            fg_color="#2a2f36",
        )
        self.chat_entry.grid(row=0, column=0, sticky="ew", padx=(14, 8), pady=12)
        self.chat_entry.bind("<Return>", lambda _e: self._enviar())

        self.btn_enviar = ctk.CTkButton(
            pie,
            text="➤",
            width=44,
            height=42,
            corner_radius=21,
            font=ctk.CTkFont(size=18, weight="bold"),
            command=self._enviar,
        )
        self.btn_enviar.grid(row=0, column=1, padx=(0, 14), pady=12)

    # -- Conversación ---------------------------------------------------
    def _mostrar_mensaje(self, texto: str, autor: str = "bot") -> ctk.CTkFrame:
        """Agrega una burbuja al historial y devuelve su fila contenedora."""
        es_usuario = autor == "usuario"

        fila = ctk.CTkFrame(self.chat_box, fg_color="transparent")
        fila.pack(fill="x", padx=2, pady=5)

        burbuja = ctk.CTkFrame(
            fila,
            fg_color=(
                self.COLOR_BURBUJA_USUARIO if es_usuario else self.COLOR_BURBUJA_BOT
            ),
            corner_radius=18,
        )

        avatar = ctk.CTkLabel(
            fila,
            text="Tú" if es_usuario else "U",
            width=32,
            height=32,
            corner_radius=16,
            fg_color="#475569" if es_usuario else self.COLOR_ACENTO,
            text_color="#ffffff",
            font=ctk.CTkFont(size=10, weight="bold"),
        )

        if es_usuario:
            avatar.pack(side="right", anchor="n", padx=(8, 0))
            burbuja.pack(side="right", anchor="e")
        else:
            avatar.pack(side="left", anchor="n", padx=(0, 8))
            burbuja.pack(side="left", anchor="w")

        ctk.CTkLabel(
            burbuja,
            text=texto,
            wraplength=330,
            justify="left",
            anchor="w",
            font=ctk.CTkFont(size=13),
            text_color="#ffffff" if es_usuario else self.COLOR_TEXTO,
        ).pack(anchor="w", padx=14, pady=(9, 2))

        ctk.CTkLabel(
            burbuja,
            text=datetime.datetime.now().strftime("%H:%M"),
            font=ctk.CTkFont(size=9),
            text_color="#bfdbfe" if es_usuario else self.COLOR_TEXTO_SUAVE,
        ).pack(anchor="e", padx=12, pady=(0, 6))

        self._ir_al_final()
        return fila

    def _ir_al_final(self) -> None:
        """Desplaza el historial hasta el último mensaje."""
        try:
            self.update_idletasks()
            canvas = getattr(self.chat_box, "_parent_canvas", None)
            if canvas is not None:
                canvas.yview_moveto(1.0)
        except Exception:  # noqa: BLE001 - la ventana pudo cerrarse
            pass

    def _enfocar_entrada(self) -> None:
        try:
            self.chat_entry.focus_set()
        except Exception:  # noqa: BLE001 - la ventana pudo cerrarse
            pass

    def _enviar(self, texto: str | None = None) -> None:
        """Envía la pregunta al backend C y muestra la respuesta como burbuja."""
        pregunta = (texto if texto is not None else self.chat_entry.get()).strip()
        if not pregunta:
            return

        self.chat_entry.delete(0, "end")
        self._mostrar_mensaje(pregunta, autor="usuario")

        self.btn_enviar.configure(state="disabled")
        indicador = self._mostrar_mensaje("Urbano está escribiendo…", autor="bot")
        self.update_idletasks()

        try:
            respuesta = ConexionC.ejecutar("chatbot", pregunta)
        finally:
            if indicador.winfo_exists():
                indicador.destroy()
            self.btn_enviar.configure(state="normal")

        self._mostrar_mensaje(self._extraer_respuesta(respuesta), autor="bot")
        self._enfocar_entrada()

    def _extraer_respuesta(self, respuesta: dict) -> str:
        """Normaliza la respuesta IPC y decodifica los saltos de línea."""
        if respuesta.get("status") == "OK":
            data = respuesta.get("data") or []
            if data and data[0]:
                # El backend codifica los saltos internos como "\n".
                mensaje = str(data[0][0]).replace("\\n", "\n").strip()
                if mensaje:
                    return mensaje
            return "No recibí una respuesta del sistema. Intenta de nuevo."

        detalle = str(respuesta.get("mensaje") or "").strip()
        if detalle in self.ERRORES_AMIGABLES:
            return self.ERRORES_AMIGABLES[detalle]
        return (
            "No pude completar tu consulta en este momento. "
            "Intenta de nuevo en unos segundos."
        )

    def _cerrar(self) -> None:
        self.destroy()



# ===========================================================================
# VistaUsuarios (CRUD de usuarios)
# ===========================================================================
class VistaUsuarios(ctk.CTkFrame):
    """Vista CRUD de usuarios: formulario (ID/Nombre/Username/Password/Rol/
    Sucursal) + tabla."""

    COLUMNAS = ("ID", "Nombre", "Username", "Rol", "Sucursal")
    ANCHOS = {
        "ID": 60,
        "Nombre": 170,
        "Username": 130,
        "Rol": 90,
        "Sucursal": 130,
    }
    ROLES = ("ADMIN", "VENDEDOR", "TECNICO")
    SUCURSALES_POR_DEFECTO = ("Sede Central", "Sede Norte")

    def __init__(self, master: ctk.CTk, id_sucursal_actual: int = 1) -> None:
        super().__init__(master, fg_color="transparent")
        self.id_sucursal_actual = id_sucursal_actual
        self._sucursales: dict[str, int] = {}
        self.grid_rowconfigure(0, weight=1)
        self.grid_rowconfigure(1, weight=1)
        self.grid_columnconfigure(0, weight=1)

        # ---- Zona superior: formulario ----
        zona = ctk.CTkFrame(self, fg_color="transparent")
        zona.grid(row=0, column=0, sticky="ew", padx=8, pady=(8, 4))

        ctk.CTkLabel(
            zona, text="Gestión de Usuarios", font=ctk.CTkFont(size=20, weight="bold")
        ).grid(row=0, column=0, sticky="w")

        # ID numérico del usuario (clave de la tabla `usuarios`). Se usa para
        # editar el rol o eliminar; no se envía al crear porque lo genera la BD.
        self.entry_id = ctk.CTkEntry(
            zona, placeholder_text="ID (para editar/eliminar)", width=180
        )
        self.entry_id.grid(row=1, column=0, sticky="w")
        self.entry_nombre = ctk.CTkEntry(zona, placeholder_text="Nombre", width=180)
        self.entry_nombre.grid(row=2, column=0, sticky="w")
        self.entry_username = ctk.CTkEntry(zona, placeholder_text="Username", width=160)
        self.entry_username.grid(row=3, column=0, sticky="w")
        self.entry_password = ctk.CTkEntry(zona, placeholder_text="Password", show="*", width=160)
        self.entry_password.grid(row=4, column=0, sticky="w")
        self.option_rol = ctk.CTkOptionMenu(zona, values=self.ROLES, width=180)
        self.option_rol.grid(row=5, column=0, sticky="w")

        # Sucursal del empleado: se carga desde la tabla `sucursal` del backend.
        # Se usa al crear y al editar (Actualizar Rol también cambia el local).
        self.option_sucursal = ctk.CTkComboBox(
            zona, values=list(self.SUCURSALES_POR_DEFECTO), width=180
        )
        self.option_sucursal.grid(row=6, column=0, sticky="w", pady=(4, 0))

        self.btn_registrar = ctk.CTkButton(
            zona, text="Registrar", command=self._on_registrar,
            fg_color="#2ea043", hover_color="#258a3d",
        )
        self.btn_registrar.grid(row=7, column=0, sticky="w", padx=(0, 8), pady=(12, 0))
        self.btn_actualizar_rol = ctk.CTkButton(
            zona, text="Actualizar Rol/Sucursal", command=self._on_actualizar_rol,
            fg_color="#2f6feb", hover_color="#2658c9",
        )
        self.btn_actualizar_rol.grid(row=7, column=1, sticky="e", padx=(8, 0), pady=(12, 0))
        self.btn_eliminar = ctk.CTkButton(
            zona, text="Eliminar", command=self._on_eliminar,
            fg_color="#e5484d", hover_color="#c93a3e",
        )
        self.btn_eliminar.grid(row=7, column=2, sticky="e", padx=(8, 0), pady=(12, 0))

        self.lbl_error = ctk.CTkLabel(zona, text="", text_color="#e5484d")
        self.lbl_error.grid(row=8, column=0, sticky="ew", pady=(0, 8))

        # ---- Zona inferior: tabla Treeview oscuro ----
        frame_tabla = ctk.CTkFrame(self, fg_color=("gray75", "gray25"), corner_radius=12)
        frame_tabla.grid(row=1, column=0, sticky="nsew", padx=8, pady=8)
        frame_tabla.grid_rowconfigure(0, weight=1)
        frame_tabla.grid_columnconfigure(0, weight=1)

        self.tree = ttk.Treeview(frame_tabla, columns=self.COLUMNAS, show="headings")
        scrollbar = ttk.Scrollbar(frame_tabla, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=scrollbar.set)
        self.tree.grid(row=0, column=0, sticky="nsew")
        scrollbar.grid(row=0, column=1, padx=(0, 4), sticky="ns")
        for col in self.COLUMNAS:
            self.tree.heading(col, text=col, anchor="w")
            self.tree.column(col, width=self.ANCHOS[col], anchor="w")
        self._configurar_estilo_treeview()
        self.tree.bind("<<TreeviewSelect>>", self._on_seleccionar)

        # Al iniciar: cargar sucursales y usuarios desde el backend C.
        self._cargar_sucursales()
        self._cargar_usuarios()

    def _on_seleccionar(self, _event=None) -> None:
        """Copia el ID y la sucursal de la fila seleccionada al formulario."""
        seleccion = self.tree.selection()
        if not seleccion:
            return
        valores = self.tree.item(seleccion[0], "values")
        if valores:
            self.entry_id.delete(0, "end")
            self.entry_id.insert(0, str(valores[0]))
            if len(valores) > 4 and str(valores[4]).strip():
                self.option_sucursal.set(str(valores[4]).strip())

    # -- Estilo Treeview (mismo tema oscuro que VistaInventario) --
    def _configurar_estilo_treeview(self) -> None:
        estilo = ttk.Style()
        estilo.theme_use("clam")
        estilo.configure(
            "Treeview",
            background="#343541",
            foreground="#DCE4EE",
            fieldbackground="#343541",
            rowheight=28,
            font=("Roboto", 12),
        )
        estilo.map(
            "Treeview",
            background=[("selected", "#4F4F4F")],
            foreground=[("selected", "#FFFFFF")],
        )
        estilo.configure(
            "Treeview.Heading",
            background="#343541",
            foreground="#DCE4EE",
            font=("Roboto", 11, "bold"),
            relief="flat",
        )
        estilo.map(
            "Treeview.Heading",
            background=[("active", "#3F3F4B")],
        )

    # -- Catálogo de sucursales para el combo --
    def _cargar_sucursales(self) -> None:
        respuesta = ConexionC.ejecutar("listar_sucursales")
        mapa: dict[str, int] = {}

        if respuesta.get("status") == "OK":
            for fila in respuesta.get("data") or []:
                valores = [str(v).strip() for v in list(fila)]
                if len(valores) >= 2 and valores[0].isdigit():
                    mapa[valores[1]] = int(valores[0])

        # Respaldo si la tabla aún no está disponible en una BD antigua.
        if not mapa:
            mapa = {
                nombre: indice
                for indice, nombre in enumerate(
                    self.SUCURSALES_POR_DEFECTO, start=1
                )
            }

        self._sucursales = mapa
        nombres = list(mapa.keys())
        self.option_sucursal.configure(values=nombres)
        actual = next(
            (
                nombre
                for nombre, id_sucursal in mapa.items()
                if id_sucursal == self.id_sucursal_actual
            ),
            nombres[0],
        )
        self.option_sucursal.set(actual)

    def _id_sucursal_seleccionada(self) -> int | None:
        nombre = self.option_sucursal.get().strip()
        id_sucursal = self._sucursales.get(nombre)
        if id_sucursal is None:
            self.lbl_error.configure(text="Seleccione una sucursal válida.")
        return id_sucursal

    # -- Carga inicial: listar_usuarios -> tabla --
    def _cargar_usuarios(self) -> None:
        # Se limpia la tabla ANTES de insertar para que el refresco visual sea
        # inmediato al crear, editar o eliminar un usuario.
        self.tree.delete(*self.tree.get_children())

        respuesta = ConexionC.ejecutar("listar_usuarios")
        if respuesta.get("status") == "OK":
            datos = respuesta.get("data") or []
            for fila in datos:
                self.tree.insert("", "end", values=self._normalizar_fila(fila))
            self.tree.update_idletasks()
            self.lbl_error.configure(text="")
        else:
            mensaje = respuesta.get("mensaje") or "No se pudieron cargar los usuarios."
            self.lbl_error.configure(text=f"Error: {mensaje}")

    # -- Helpers --
    def _normalizar_fila(self, fila) -> tuple[str, ...]:
        """Backend: [id, nombre, username, rol, id_sucursal, nombre_sucursal].
        Tabla:    [id, nombre, username, rol, nombre_sucursal]."""
        valores = [str(v).strip() for v in list(fila)]
        if len(valores) >= 6:
            return tuple(valores[:4] + [valores[5]])
        valores = valores[: len(self.COLUMNAS)]
        valores += [""] * (len(self.COLUMNAS) - len(valores))
        return tuple(valores)

    def _limpiar_campos(self) -> None:
        """Vacía el formulario tras una operación exitosa."""
        for entry in (
            self.entry_id,
            self.entry_nombre,
            self.entry_username,
            self.entry_password,
        ):
            entry.delete(0, "end")
        self.option_rol.set(self.ROLES[0])
        actual = next(
            (
                nombre
                for nombre, id_sucursal in self._sucursales.items()
                if id_sucursal == self.id_sucursal_actual
            ),
            next(iter(self._sucursales), self.SUCURSALES_POR_DEFECTO[0]),
        )
        self.option_sucursal.set(actual)

    def _refrescar_tras_cambio(self) -> None:
        """Limpia el formulario y fuerza un refresco inmediato de la tabla."""
        self._limpiar_campos()
        self._cargar_usuarios()
        self.update_idletasks()

    # -- Registrar usuario --
    def _on_registrar(self) -> None:
        nombre = self.entry_nombre.get().strip()
        user = self.entry_username.get().strip()
        pasw = self.entry_password.get()
        rol = self.option_rol.get()
        id_sucursal = self._id_sucursal_seleccionada()

        if not nombre or not user or not pasw or not rol or id_sucursal is None:
            self.lbl_error.configure(text="Complete todos los campos.")
            return

        # main.c espera exactamente:
        #   crear_usuario <nombre> <username> <password> <rol> <id_sucursal>
        respuesta = ConexionC.ejecutar(
            "crear_usuario",
            nombre,
            user,
            pasw,
            rol,
            sucursal=id_sucursal,
        )

        if respuesta.get("status") == "OK":
            self.lbl_error.configure(text="")
            self._refrescar_tras_cambio()
        else:
            mensaje = respuesta.get("mensaje") or "No se pudo registrar el usuario."
            self.lbl_error.configure(text=f"Error: {mensaje}")

    # -- Actualizar rol --
    def _on_actualizar_rol(self) -> None:
        id_usuario = self.entry_id.get().strip()
        rol = self.option_rol.get()
        id_sucursal = self._id_sucursal_seleccionada()

        if not id_usuario or not rol or id_sucursal is None:
            self.lbl_error.configure(
                text="Ingrese el ID, seleccione un rol y una sucursal."
            )
            return
        if not id_usuario.isdigit():
            self.lbl_error.configure(text="El ID debe ser numérico.")
            return

        # main.c espera exactamente: editar_usuario <id> <rol> <id_sucursal>
        respuesta = ConexionC.ejecutar(
            "editar_usuario", id_usuario, rol, sucursal=id_sucursal
        )

        if respuesta.get("status") == "OK":
            self.lbl_error.configure(text="")
            self._refrescar_tras_cambio()
        else:
            mensaje = respuesta.get("mensaje") or "No se pudo actualizar el rol."
            self.lbl_error.configure(text=f"Error: {mensaje}")

    # -- Eliminar usuario --
    def _on_eliminar(self) -> None:
        id_usuario = self.entry_id.get().strip()

        if not id_usuario:
            self.lbl_error.configure(text="Ingrese el ID del usuario a eliminar.")
            return
        if not id_usuario.isdigit():
            self.lbl_error.configure(text="El ID debe ser numérico.")
            return

        # main.c espera exactamente: eliminar_usuario <id> <id_sucursal>
        respuesta = ConexionC.ejecutar("eliminar_usuario", id_usuario)

        if respuesta.get("status") == "OK":
            self.lbl_error.configure(text="")
            self._refrescar_tras_cambio()
        else:
            mensaje = respuesta.get("mensaje") or "No se pudo eliminar el usuario."
            self.lbl_error.configure(text=f"Error: {mensaje}")


# ===========================================================================
# FarmaciaApp: controlador raiz
# ===========================================================================
class FarmaciaApp(ctk.CTk):
    def __init__(self) -> None:
        super().__init__()
        self.title("Farmacia Urbana")
        self.geometry("1100x650")
        self.minsize(900, 550)

        self.usuario_actual: str | None = None
        self.id_sucursal_actual: int | None = None
        self.nombre_sucursal_actual: str | None = None
        self.current_frame: ctk.CTkFrame | None = None

        self.mostrar_login()

    def _cambiar_frame(self, nuevo: ctk.CTkFrame) -> None:
        if self.current_frame is not None:
            self.current_frame.destroy()
        self.current_frame = nuevo
        self.current_frame.pack(fill="both", expand=True)

    def mostrar_login(self) -> None:
        self.usuario_actual = None
        self.id_sucursal_actual = None
        self.nombre_sucursal_actual = None
        ConexionC.limpiar_sucursal()
        os.environ.pop("FARMACIA_USER", None)
        os.environ.pop("FARMACIA_SUCURSAL", None)
        self.title("Farmacia Urbana")
        self._cambiar_frame(LoginFrame(self, on_success=self.mostrar_dashboard))

    def mostrar_dashboard(
        self,
        usuario: str,
        rol: str = "VENDEDOR",
        id_sucursal: str = "1",
        nombre_sucursal: str = "Sede Central",
    ) -> None:
        self.usuario_actual = usuario
        self.id_sucursal_actual = int(id_sucursal)
        self.nombre_sucursal_actual = nombre_sucursal

        # La sucursal queda fijada en la sesión: ConexionC la agrega a cada
        # petición IPC (inventario, ventas, reportes, alertas y chatbot).
        ConexionC.establecer_sucursal(self.id_sucursal_actual)
        # El backend C usa estas variables para resolver venta.id_vendedor.
        os.environ["FARMACIA_USER"] = usuario
        os.environ["FARMACIA_SUCURSAL"] = str(self.id_sucursal_actual)

        self.title(f"Farmacia Urbana - {nombre_sucursal}")
        self._cambiar_frame(
            DashboardFrame(
                self,
                usuario=usuario,
                on_logout=self.mostrar_login,
                rol=rol,
                id_sucursal=self.id_sucursal_actual,
                nombre_sucursal=nombre_sucursal,
            )
        )


def main() -> None:
    app = FarmaciaApp()
    app.mainloop()


if __name__ == "__main__":
    main()
