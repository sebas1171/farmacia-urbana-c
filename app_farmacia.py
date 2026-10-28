"""Farmacia Urbana — frontend CustomTkinter (app_farmacia.py).

Estructura base:
    FarmaciaApp (controlador raiz / ctk.CTk)
      |- LoginFrame        -> autenticacion via ConexionC.ejecutar("login", ...)
      |- DashboardFrame   -> sidebar lateral + area central con vistas

Backend: binario C ./farmacia, IPC normalizada por integracion_ipc.ConexionC.
"""

from __future__ import annotations

import customtkinter as ctk
from tkinter import ttk
from typing import Callable

from integracion_ipc import ConexionC


ctk.set_appearance_mode("dark")
ctk.set_default_color_theme("blue")


# ===========================================================================
# LoginFrame
# ===========================================================================
class LoginFrame(ctk.CTkFrame):
    """Pantalla inicial: usuario, contrasena y boton de ingreso."""

    def __init__(self, master: ctk.CTk, on_success: Callable[[str], None]) -> None:
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
            # Éxito: el controlador destruye este frame y muestra el dashboard.
            self._on_success(usuario)
            return

        # ERROR o FATAL: se muestra el mensaje en el label rojo.
        mensaje = respuesta.get("mensaje") or "No se pudo completar el inicio de sesion."
        self.lbl_error.configure(text=mensaje)


# ===========================================================================
# VistaInventario (búsqueda de productos)
# ===========================================================================
class VistaInventario(ctk.CTkFrame):
    """Vista de busqueda de productos: Entry + Treeview (ttk styled).

    Layout:
        Fila 0: barra superior (Entry de búsqueda + boton 'Buscar')
        Fila 1: tabla Treeview con scroll
        Fila 2: label de estado / errores
    """

    COLUMNAS = ("Código", "Nombre", "Precio", "Stock", "Vencimiento")
    ANCHOS = {
        "Código": 80,
        "Nombre": 180,
        "Precio": 90,
        "Stock": 70,
        "Vencimiento": 110,
    }

    def __init__(self, master: ctk.CTk) -> None:
        super().__init__(master, fg_color="transparent")
        self.grid_rowconfigure(0, weight=1)
        self.grid_columnconfigure(0, weight=1)

        # ---- Barra superior: busqueda ----
        barra = ctk.CTkFrame(self, fg_color="transparent")
        barra.grid(row=0, column=0, sticky="ew", padx=8, pady=(8, 4))
        barra.grid_columnconfigure(0, weight=1)

        self.entry_busqueda = ctk.CTkEntry(barra, placeholder_text="Buscar producto...")
        self.entry_busqueda.grid(row=0, column=0, sticky="ew", padx=(0, 8))

        self.btn_buscar = ctk.CTkButton(
            barra, text="Buscar", width=120, command=self._on_buscar
        )
        self.btn_buscar.grid(row=0, column=1)

        # ---- Tabla (Treeview con scroll) ----
        frame_tabla = ctk.CTkFrame(
            self, fg_color=("gray75", "gray25"), corner_radius=12
        )
        frame_tabla.grid(row=1, column=0, sticky="nsew", padx=8, pady=8)
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

        # ---- Estado ----
        self.lbl_estado = ctk.CTkLabel(
            self, text="", font=ctk.CTkFont(size=13), text_color="#e5484d"
        )
        self.lbl_estado.grid(row=2, column=0, sticky="ew", padx=8, pady=(0, 6))

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
            self.lbl_estado.configure(text="")
        else:
            mensaje = respuesta.get("mensaje") or "No se encontraron resultados."
            self.lbl_estado.configure(text=f"Error: {mensaje}")

    # -- Helpers --
    def _normalizar_fila(self, fila) -> tuple[str, ...]:
        """Asegura una fila de longitud completa para los 5 columnas."""
        valores = [str(v).strip() for v in list(fila)[: len(self.COLUMNAS)]]
        valores += [""] * (len(self.COLUMNAS) - len(valores))
        return tuple(valores)


# ===========================================================================
# VistaVentas (carrito de compras + cobro)
# ===========================================================================
class VistaVentas(ctk.CTkFrame):
    """Vista de ventas: agregar productos al carrito, ver total y procesar."""

    COLUMNAS = ("Código", "Nombre", "Precio Unit.", "Cantidad", "Subtotal")

    def __init__(self, master: ctk.CTk) -> None:
        super().__init__(master, fg_color="transparent")
        self.grid_rowconfigure(0, weight=1)
        self.grid_columnconfigure(0, weight=1)

        # Zona superior (Agregar al carrito)
        top = ctk.CTkFrame(self, fg_color="transparent")
        top.grid(row=0, column=0, sticky="ew", padx=8, pady=(8, 4))
        top.grid_columnconfigure(0, weight=1)
        top.grid_columnconfigure(1, weight=0)

        self.entry_codigo = ctk.CTkEntry(
            top, placeholder_text="Código del producto (ej. MED-001)"
        )
        self.entry_codigo.grid(row=0, column=0, sticky="ew", padx=(0, 8))
        self.entry_cantidad = ctk.CTkEntry(top, placeholder_text="1", width=100)
        self.entry_cantidad.grid(row=0, column=1)
        self.btn_agregar = ctk.CTkButton(
            top, text="Agregar al Carrito", command=self._on_agregar
        )
        self.btn_agregar.grid(row=0, column=2)

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

    # -- Zona superior: agregar al carrito --
    def _on_agregar(self) -> None:
        codigo = self.entry_codigo.get().strip()
        try:
            cantidad = int(float(self.entry_cantidad.get().strip() or 1))
        except ValueError:
            cantidad = 1
        if cantidad <= 0:
            self.lbl_error.configure(text="Cantidad inválida.")
            return

        respuesta = ConexionC.ejecutar("buscar_prod_codigo", codigo)

        if respuesta.get("status") != "OK":
            mensaje = respuesta.get("mensaje") or "Producto no encontrado."
            self.lbl_error.configure(text=f"Error: {mensaje}")
            return

        data = respuesta.get("data") or []
        # Contrato: data = [[Nombre, Precio, Stock]] -> tomamos la primera fila
        fila = data[0] if len(data) > 0 else []
        nombre = str(fila[0]) if len(fila) > 0 else codigo
        try:
            precio = float(fila[1]) if len(fila) > 1 else 0.0
        except (ValueError, IndexError):
            precio = 0.0
        subtotal = round(precio * cantidad, 2)

        self.tree.insert(
            "",
            "end",
            values=(codigo, nombre, f"{precio:.2f}", str(cantidad), f"{subtotal:.2f}"),
        )
        self.lbl_error.configure(text="")
        self.lbl_total.configure(text=f"TOTAL: S/ {self._calcular_total():.2f}")

    # -- Zona inferior: procesar venta --
    def _on_procesar(self) -> None:
        items: list[str] = []
        for item in self.tree.get_children():
            v = self.tree.item(item, "values")
            try:
                codigo = str(v[0]).strip()
                cantidad = int(float(str(v[3]).strip()))
            except (ValueError, IndexError):
                continue
            items.append(f"{codigo},{cantidad}")

        carrito = "|".join(items)
        dni = self.entry_dni.get().strip()

        respuesta = ConexionC.ejecutar("procesar_venta", dni, carrito)

        if respuesta.get("status") == "OK":
            self.tree.delete(*self.tree.get_children())
            self.lbl_total.configure(text="TOTAL: S/ 0.00")
            self.lbl_exito.configure(text="Venta Exitosa")
            self.lbl_error.configure(text="")
        else:
            mensaje = respuesta.get("mensaje") or "No se pudo procesar la venta."
            self.lbl_error.configure(text=f"Error: {mensaje}")

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

    COLUMNAS = ("Fecha", "Vendedor", "Producto", "Cantidad", "Subtotal")
    ANCHOS = {
        "Fecha": 100,
        "Vendedor": 120,
        "Producto": 150,
        "Cantidad": 80,
        "Subtotal": 90,
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

        respuesta = ConexionC.ejecutar("historial", fecha_inicio, fecha_fin)

        if respuesta.get("status") == "OK":
            datos = respuesta.get("data") or []
            self.tree.delete(*self.tree.get_children())
            for fila in datos:
                self.tree.insert("", "end", values=self._normalizar_fila(fila))
        else:
            self.lbl_error.configure(
                text=f"Error: {respuesta.get('mensaje') or 'No se generó el historial.'}"
            )

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

    def _on_refrescar(self) -> None:
        respuesta = ConexionC.ejecutar("alertas")
        if respuesta.get("status") == "OK":
            datos = respuesta.get("data") or []
            self.tree_stock.delete(*self.tree_stock.get_children())
            for fila in datos:
                vals = [str(v).strip() for v in list(fila)[:3]]
                vals += [""] * (3 - len(vals))
                self.tree_stock.insert("", "end", values=tuple(vals))
        else:
            self.lbl_error.configure(
                text=f"Error: {respuesta.get('mensaje') or 'No se pudieron cargar las alertas.'}"
            )


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
    ) -> None:
        super().__init__(master, fg_color="transparent")
        self._on_logout_cb = on_logout
        self._vista_actual: ctk.CTkFrame | None = None

        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(0, weight=1)

        # ---- Sidebar izquierda ----
        sidebar = ctk.CTkFrame(self, width=220, corner_radius=0)
        sidebar.grid(row=0, column=0, sticky="nsew")
        sidebar.grid_rowconfigure(0, weight=0)
        sidebar.grid_rowconfigure(1, weight=0)
        sidebar.grid_rowconfigure(2, weight=0)
        sidebar.grid_rowconfigure(3, weight=0)
        sidebar.grid_rowconfigure(4, weight=0)
        sidebar.grid_rowconfigure(5, weight=0)
        sidebar.grid_rowconfigure(6, weight=1)  # fila 6: resorte transparente (weight=1)
        sidebar.grid_rowconfigure(7, weight=0)

        ctk.CTkLabel(
            sidebar, text="Farmacia Urbana", font=ctk.CTkFont(size=18, weight="bold")
        ).grid(row=0, column=0, padx=20, pady=(24, 4))
        ctk.CTkLabel(sidebar, text=f"Usuario: {usuario}").grid(
            row=0, column=0, padx=20, pady=(0, 20)
        )

        self._botones: dict[str, ctk.CTkButton] = {}
        fila = 1
        for modulo in self.MODULOS:
            btn = ctk.CTkButton(
                sidebar,
                text=modulo,
                anchor="w",
                fg_color="transparent",
                text_color=("gray10", "gray90"),
                hover_color=("gray70", "gray30"),
                command=lambda m=modulo: self.mostrar_vista(m),
            )
            btn.grid(row=fila, column=0, padx=16, pady=6, sticky="ew")
            self._botones[modulo] = btn
            fila += 1

        # Resorte: CTkFrame transparente en fila 6 con weight=1 -> empuja el cierre al fondo
        resorte = ctk.CTkFrame(sidebar, fg_color="transparent", corner_radius=0)
        resorte.grid(row=6, column=0, sticky="nsew")

        ctk.CTkButton(
            sidebar,
            text="Cerrar sesion",
            fg_color="#e5484d",
            hover_color="#c93a3e",
            command=self._cerrar_sesion,
        ).grid(row=7, column=0, padx=16, pady=20, sticky="ew")

        # ---- Area central dinamica ----
        self.contenedor = ctk.CTkFrame(
            self, fg_color="transparent", corner_radius=12
        )
        self.contenedor.grid(
            row=0, column=1, sticky="nsew", padx=16, pady=16
        )
        self.contenedor.grid_columnconfigure(0, weight=1)
        self.contenedor.grid_rowconfigure(0, weight=1)

        # ---- Boton Chatbot en una esquina ----
        self.btn_chatbot = ctk.CTkButton(
            self, text="💬 Chatbot", command=self._abrir_chatbot
        )
        self.btn_chatbot.place(relx=1.0, rely=0.0, anchor="ne", x=16, y=16)

        self.mostrar_vista(self.MODULOS[0])

    # -- Navegacion --
    def mostrar_vista(self, modulo: str) -> None:
        """Destruye la vista actual e instancia un placeholder con el titulo."""
        if self._vista_actual is not None:
            self._vista_actual.destroy()
            self._vista_actual = None

        if modulo == "Inventario":
            frame = VistaInventario(self.contenedor)
        elif modulo == "Ventas":
            frame = VistaVentas(self.contenedor)
        elif modulo == "Reportes":
            frame = VistaReportes(self.contenedor)
        elif modulo == "Alertas":
            frame = VistaAlertas(self.contenedor)
        elif modulo == "Usuarios":
            frame = VistaUsuarios(self.contenedor)
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
    """Ventana emergente del chatbot: historial + entrada de pregunta."""

    def __init__(self, master: ctk.CTk) -> None:
        super().__init__(master)
        self.title("Asistente Virtual")
        self.geometry("460x560")
        self.grid_rowconfigure(0, weight=1)
        self.grid_columnconfigure(0, weight=1)

        self.chat_box = ctk.CTkTextbox(self, wrap="word")
        self.chat_box.grid(row=0, column=0, sticky="nsew", padx=12, pady=12)
        self.chat_box.configure(state="disabled")
        self.chat_box.insert("end", "Hola, soy el asistente de Farmacia Urbana.\n")

        bottom = ctk.CTkFrame(self, fg_color="transparent")
        bottom.grid(row=1, column=0, sticky="ew", padx=12, pady=(0, 12))
        bottom.grid_columnconfigure(0, weight=1)

        self.chat_entry = ctk.CTkEntry(bottom, placeholder_text="Escribe su pregunta...")
        self.chat_entry.grid(row=0, column=0, sticky="ew", padx=(0, 8))
        self.btn_enviar = ctk.CTkButton(bottom, text="Enviar", command=self._enviar)
        self.btn_enviar.grid(row=0, column=1)
        self.chat_entry.bind("<Return>", lambda _e: self._enviar())

        self.protocol("WM_DELETE_WINDOW", self._cerrar)

    def _enviar(self) -> None:
        pregunta = self.chat_entry.get().strip()
        if not pregunta:
            return

        # Inserta el mensaje del usuario
        self.chat_box.configure(state="normal")
        self.chat_box.insert("end", f"Tú: {pregunta}\n")
        self.chat_box.configure(state="disabled")
        self.chat_entry.delete(0, "end")

        # Consulta al backend C
        respuesta = ConexionC.ejecutar("chatbot", pregunta)
        data = respuesta.get("data") or []
        bot_msg = str(data[0][0]) if data and data[0] else "Sin respuesta."

        # Inserta la respuesta del bot
        self.chat_box.configure(state="normal")
        self.chat_box.insert("end", f"Bot: {bot_msg}\n")
        self.chat_box.configure(state="disabled")

    def _cerrar(self) -> None:
        self.destroy()


# ===========================================================================
# VistaUsuarios (CRUD de usuarios)
# ===========================================================================
class VistaUsuarios(ctk.CTkFrame):
    """Vista CRUD de usuarios: formulario (DNI/Nombre/Username/Password/Rol) + tabla."""

    COLUMNAS = ("DNI", "Nombre", "Username", "Rol")
    ANCHOS = {"DNI": 90, "Nombre": 150, "Username": 120, "Rol": 90}
    ROLES = ("ADMINISTRADOR", "TECNICO", "VENDEDOR")

    def __init__(self, master: ctk.CTk) -> None:
        super().__init__(master, fg_color="transparent")
        self.grid_rowconfigure(0, weight=1)
        self.grid_rowconfigure(1, weight=1)
        self.grid_columnconfigure(0, weight=1)

        # ---- Zona superior: formulario ----
        zona = ctk.CTkFrame(self, fg_color="transparent")
        zona.grid(row=0, column=0, sticky="ew", padx=8, pady=(8, 4))

        ctk.CTkLabel(
            zona, text="Gestión de Usuarios", font=ctk.CTkFont(size=20, weight="bold")
        ).grid(row=0, column=0, sticky="w")

        self.entry_dni = ctk.CTkEntry(zona, placeholder_text="DNI", width=150)
        self.entry_dni.grid(row=1, column=0, sticky="w")
        self.entry_nombre = ctk.CTkEntry(zona, placeholder_text="Nombre", width=180)
        self.entry_nombre.grid(row=2, column=0, sticky="w")
        self.entry_username = ctk.CTkEntry(zona, placeholder_text="Username", width=160)
        self.entry_username.grid(row=3, column=0, sticky="w")
        self.entry_password = ctk.CTkEntry(zona, placeholder_text="Password", show="*", width=160)
        self.entry_password.grid(row=4, column=0, sticky="w")
        self.option_rol = ctk.CTkOptionMenu(zona, values=self.ROLES, width=180)
        self.option_rol.grid(row=5, column=0, sticky="w")

        self.btn_registrar = ctk.CTkButton(
            zona, text="Registrar", command=self._on_registrar,
            fg_color="#2ea043", hover_color="#258a3d",
        )
        self.btn_registrar.grid(row=6, column=0, sticky="w", padx=(0, 8), pady=(12, 0))
        self.btn_actualizar_rol = ctk.CTkButton(
            zona, text="Actualizar Rol", command=self._on_actualizar_rol,
            fg_color="#2f6feb", hover_color="#2658c9",
        )
        self.btn_actualizar_rol.grid(row=6, column=1, sticky="e", padx=(8, 0), pady=(12, 0))
        self.btn_eliminar = ctk.CTkButton(
            zona, text="Eliminar", command=self._on_eliminar,
            fg_color="#e5484d", hover_color="#c93a3e",
        )
        self.btn_eliminar.grid(row=6, column=2, sticky="e", padx=(8, 0), pady=(12, 0))

        self.lbl_error = ctk.CTkLabel(zona, text="", text_color="#e5484d")
        self.lbl_error.grid(row=7, column=0, sticky="ew", pady=(0, 8))

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

        # Al iniciar: cargar usuarios desde el backend C
        self._cargar_usuarios()

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

    # -- Carga inicial: listar_usuarios -> tabla --
    def _cargar_usuarios(self) -> None:
        respuesta = ConexionC.ejecutar("listar_usuarios")
        if respuesta.get("status") == "OK":
            datos = respuesta.get("data") or []
            self.tree.delete(*self.tree.get_children())
            for fila in datos:
                self.tree.insert("", "end", values=self._normalizar_fila(fila))
            self.lbl_error.configure(text="")
        else:
            mensaje = respuesta.get("mensaje") or "No se pudieron cargar los usuarios."
            self.lbl_error.configure(text=f"Error: {mensaje}")

    # -- Helpers --
    def _normalizar_fila(self, fila) -> tuple[str, ...]:
        valores = [str(v).strip() for v in list(fila)[: len(self.COLUMNAS)]]
        valores += [""] * (len(self.COLUMNAS) - len(valores))
        return tuple(valores)

    # -- Registrar usuario --
    def _on_registrar(self) -> None:
        dni = self.entry_dni.get().strip()
        nombre = self.entry_nombre.get().strip()
        user = self.entry_username.get().strip()
        pasw = self.entry_password.get()
        rol = self.option_rol.get()

        if not dni or not nombre or not user or not pasw or not rol:
            self.lbl_error.configure(text="Complete todos los campos.")
            return

        respuesta = ConexionC.ejecutar("crear_usuario", dni, nombre, user, pasw, rol)

        if respuesta.get("status") == "OK":
            self.lbl_error.configure(text="")
            self._cargar_usuarios()
        else:
            mensaje = respuesta.get("mensaje") or "No se pudo registrar el usuario."
            self.lbl_error.configure(text=f"Error: {mensaje}")

    # -- Actualizar rol --
    def _on_actualizar_rol(self) -> None:
        dni = self.entry_dni.get().strip()
        rol = self.option_rol.get()

        if not dni or not rol:
            self.lbl_error.configure(text="Ingrese el DNI y seleccione un rol.")
            return

        respuesta = ConexionC.ejecutar("editar_usuario", dni, rol)

        if respuesta.get("status") == "OK":
            self.lbl_error.configure(text="")
            self._cargar_usuarios()
        else:
            mensaje = respuesta.get("mensaje") or "No se pudo actualizar el rol."
            self.lbl_error.configure(text=f"Error: {mensaje}")

    # -- Eliminar usuario --
    def _on_eliminar(self) -> None:
        dni = self.entry_dni.get().strip()

        if not dni:
            self.lbl_error.configure(text="Ingrese el DNI del usuario a eliminar.")
            return

        respuesta = ConexionC.ejecutar("eliminar_usuario", dni)

        if respuesta.get("status") == "OK":
            self.lbl_error.configure(text="")
            self._cargar_usuarios()
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
        self.current_frame: ctk.CTkFrame | None = None

        self.mostrar_login()

    def _cambiar_frame(self, nuevo: ctk.CTkFrame) -> None:
        if self.current_frame is not None:
            self.current_frame.destroy()
        self.current_frame = nuevo
        self.current_frame.pack(fill="both", expand=True)

    def mostrar_login(self) -> None:
        self.usuario_actual = None
        self._cambiar_frame(LoginFrame(self, on_success=self.mostrar_dashboard))

    def mostrar_dashboard(self, usuario: str) -> None:
        self.usuario_actual = usuario
        self._cambiar_frame(
            DashboardFrame(self, usuario=usuario, on_logout=self.mostrar_login)
        )


def main() -> None:
    app = FarmaciaApp()
    app.mainloop()


if __name__ == "__main__":
    main()
