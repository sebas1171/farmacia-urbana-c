--
-- PostgreSQL database dump
--

\restrict LdGqOPqkcUmWbJN7F1Dge7E9gWMf3MfNTmhGDmqEMRMVFIp8f5F1J1Oioi28hTq

-- Dumped from database version 16.15 (Debian 16.15-1.pgdg13+2)
-- Dumped by pg_dump version 16.15

-- Started on 2026-11-11 19:22:18 UTC

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- TOC entry 222 (class 1259 OID 16474)
-- Name: detalle_venta; Type: TABLE; Schema: public; Owner: administrador
--

CREATE TABLE public.detalle_venta (
    id_venta integer NOT NULL,
    codigo_producto character varying(13) NOT NULL,
    cantidad integer NOT NULL,
    subtotal numeric(10,2) NOT NULL
);


ALTER TABLE public.detalle_venta OWNER TO administrador;

--
-- TOC entry 217 (class 1259 OID 16443)
-- Name: inventario; Type: TABLE; Schema: public; Owner: administrador
--

CREATE TABLE public.inventario (
    id integer NOT NULL,
    nombre character varying(100) NOT NULL,
    precio numeric(10,2) NOT NULL,
    stock integer DEFAULT 0 NOT NULL
);


ALTER TABLE public.inventario OWNER TO administrador;

--
-- TOC entry 216 (class 1259 OID 16442)
-- Name: inventario_id_seq; Type: SEQUENCE; Schema: public; Owner: administrador
--

CREATE SEQUENCE public.inventario_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.inventario_id_seq OWNER TO administrador;

--
-- TOC entry 3465 (class 0 OID 0)
-- Dependencies: 216
-- Name: inventario_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: administrador
--

ALTER SEQUENCE public.inventario_id_seq OWNED BY public.inventario.id;


--
-- TOC entry 215 (class 1259 OID 16397)
-- Name: producto; Type: TABLE; Schema: public; Owner: administrador
--

CREATE TABLE public.producto (
    codigo_producto character varying(13) NOT NULL,
    nombre character varying(100) NOT NULL,
    precio numeric(10,2) NOT NULL,
    stock integer DEFAULT 0 NOT NULL,
    requiere_receta boolean DEFAULT false,
    aplica_igv boolean DEFAULT true,
    lote character varying(50),
    fecha_vencimiento date
);


ALTER TABLE public.producto OWNER TO administrador;

--
-- TOC entry 219 (class 1259 OID 16452)
-- Name: usuarios; Type: TABLE; Schema: public; Owner: administrador
--

CREATE TABLE public.usuarios (
    id integer NOT NULL,
    username character varying(32) NOT NULL,
    password character varying(32) NOT NULL,
    nombre character varying(100) NOT NULL,
    rol character varying(16) NOT NULL,
    activo boolean DEFAULT true
);


ALTER TABLE public.usuarios OWNER TO administrador;

--
-- TOC entry 218 (class 1259 OID 16451)
-- Name: usuarios_id_seq; Type: SEQUENCE; Schema: public; Owner: administrador
--

CREATE SEQUENCE public.usuarios_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.usuarios_id_seq OWNER TO administrador;

--
-- TOC entry 3466 (class 0 OID 0)
-- Dependencies: 218
-- Name: usuarios_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: administrador
--

ALTER SEQUENCE public.usuarios_id_seq OWNED BY public.usuarios.id;


--
-- TOC entry 221 (class 1259 OID 16462)
-- Name: venta; Type: TABLE; Schema: public; Owner: administrador
--

CREATE TABLE public.venta (
    id integer NOT NULL,
    fecha_hora timestamp without time zone DEFAULT CURRENT_TIMESTAMP,
    id_vendedor integer,
    dni_cliente character varying(8),
    total numeric(10,2) NOT NULL
);


ALTER TABLE public.venta OWNER TO administrador;

--
-- TOC entry 220 (class 1259 OID 16461)
-- Name: venta_id_seq; Type: SEQUENCE; Schema: public; Owner: administrador
--

CREATE SEQUENCE public.venta_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.venta_id_seq OWNER TO administrador;

--
-- TOC entry 3467 (class 0 OID 0)
-- Dependencies: 220
-- Name: venta_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: administrador
--

ALTER SEQUENCE public.venta_id_seq OWNED BY public.venta.id;


--
-- TOC entry 3288 (class 2604 OID 16446)
-- Name: inventario id; Type: DEFAULT; Schema: public; Owner: administrador
--

ALTER TABLE ONLY public.inventario ALTER COLUMN id SET DEFAULT nextval('public.inventario_id_seq'::regclass);


--
-- TOC entry 3290 (class 2604 OID 16455)
-- Name: usuarios id; Type: DEFAULT; Schema: public; Owner: administrador
--

ALTER TABLE ONLY public.usuarios ALTER COLUMN id SET DEFAULT nextval('public.usuarios_id_seq'::regclass);


--
-- TOC entry 3292 (class 2604 OID 16465)
-- Name: venta id; Type: DEFAULT; Schema: public; Owner: administrador
--

ALTER TABLE ONLY public.venta ALTER COLUMN id SET DEFAULT nextval('public.venta_id_seq'::regclass);


--
-- TOC entry 3459 (class 0 OID 16474)
-- Dependencies: 222
-- Data for Name: detalle_venta; Type: TABLE DATA; Schema: public; Owner: administrador
--

COPY public.detalle_venta (id_venta, codigo_producto, cantidad, subtotal) FROM stdin;
\.


--
-- TOC entry 3454 (class 0 OID 16443)
-- Dependencies: 217
-- Data for Name: inventario; Type: TABLE DATA; Schema: public; Owner: administrador
--

COPY public.inventario (id, nombre, precio, stock) FROM stdin;
1	paracetamol	2.50	100
2	ibuprofeno	3.00	50
3	amoxicilina	5.50	30
\.


--
-- TOC entry 3452 (class 0 OID 16397)
-- Dependencies: 215
-- Data for Name: producto; Type: TABLE DATA; Schema: public; Owner: administrador
--

COPY public.producto (codigo_producto, nombre, precio, stock, requiere_receta, aplica_igv, lote, fecha_vencimiento) FROM stdin;
7751234567890	Paracetamol 500mg	2.50	100	f	t	\N	\N
7759876543210	Ibuprofeno 400mg	3.00	50	f	t	\N	\N
7754567890123	Amoxicilina 500mg	5.50	30	f	t	\N	\N
\.


--
-- TOC entry 3456 (class 0 OID 16452)
-- Dependencies: 219
-- Data for Name: usuarios; Type: TABLE DATA; Schema: public; Owner: administrador
--

COPY public.usuarios (id, username, password, nombre, rol, activo) FROM stdin;
1	admin	admin123	Sebastián Herrera	ADMIN	t
2	vendedor1	vend123	Paolo Castro	VENDEDOR	t
\.


--
-- TOC entry 3458 (class 0 OID 16462)
-- Dependencies: 221
-- Data for Name: venta; Type: TABLE DATA; Schema: public; Owner: administrador
--

COPY public.venta (id, fecha_hora, id_vendedor, dni_cliente, total) FROM stdin;
\.


--
-- TOC entry 3468 (class 0 OID 0)
-- Dependencies: 216
-- Name: inventario_id_seq; Type: SEQUENCE SET; Schema: public; Owner: administrador
--

SELECT pg_catalog.setval('public.inventario_id_seq', 3, true);


--
-- TOC entry 3469 (class 0 OID 0)
-- Dependencies: 218
-- Name: usuarios_id_seq; Type: SEQUENCE SET; Schema: public; Owner: administrador
--

SELECT pg_catalog.setval('public.usuarios_id_seq', 2, true);


--
-- TOC entry 3470 (class 0 OID 0)
-- Dependencies: 220
-- Name: venta_id_seq; Type: SEQUENCE SET; Schema: public; Owner: administrador
--

SELECT pg_catalog.setval('public.venta_id_seq', 1, false);


--
-- TOC entry 3305 (class 2606 OID 16478)
-- Name: detalle_venta detalle_venta_pkey; Type: CONSTRAINT; Schema: public; Owner: administrador
--

ALTER TABLE ONLY public.detalle_venta
    ADD CONSTRAINT detalle_venta_pkey PRIMARY KEY (id_venta, codigo_producto);


--
-- TOC entry 3297 (class 2606 OID 16449)
-- Name: inventario inventario_pkey; Type: CONSTRAINT; Schema: public; Owner: administrador
--

ALTER TABLE ONLY public.inventario
    ADD CONSTRAINT inventario_pkey PRIMARY KEY (id);


--
-- TOC entry 3295 (class 2606 OID 16404)
-- Name: producto producto_pkey; Type: CONSTRAINT; Schema: public; Owner: administrador
--

ALTER TABLE ONLY public.producto
    ADD CONSTRAINT producto_pkey PRIMARY KEY (codigo_producto);


--
-- TOC entry 3299 (class 2606 OID 16458)
-- Name: usuarios usuarios_pkey; Type: CONSTRAINT; Schema: public; Owner: administrador
--

ALTER TABLE ONLY public.usuarios
    ADD CONSTRAINT usuarios_pkey PRIMARY KEY (id);


--
-- TOC entry 3301 (class 2606 OID 16460)
-- Name: usuarios usuarios_username_key; Type: CONSTRAINT; Schema: public; Owner: administrador
--

ALTER TABLE ONLY public.usuarios
    ADD CONSTRAINT usuarios_username_key UNIQUE (username);


--
-- TOC entry 3303 (class 2606 OID 16468)
-- Name: venta venta_pkey; Type: CONSTRAINT; Schema: public; Owner: administrador
--

ALTER TABLE ONLY public.venta
    ADD CONSTRAINT venta_pkey PRIMARY KEY (id);


--
-- TOC entry 3307 (class 2606 OID 16484)
-- Name: detalle_venta detalle_venta_codigo_producto_fkey; Type: FK CONSTRAINT; Schema: public; Owner: administrador
--

ALTER TABLE ONLY public.detalle_venta
    ADD CONSTRAINT detalle_venta_codigo_producto_fkey FOREIGN KEY (codigo_producto) REFERENCES public.producto(codigo_producto);


--
-- TOC entry 3308 (class 2606 OID 16479)
-- Name: detalle_venta detalle_venta_id_venta_fkey; Type: FK CONSTRAINT; Schema: public; Owner: administrador
--

ALTER TABLE ONLY public.detalle_venta
    ADD CONSTRAINT detalle_venta_id_venta_fkey FOREIGN KEY (id_venta) REFERENCES public.venta(id);


--
-- TOC entry 3306 (class 2606 OID 16469)
-- Name: venta venta_id_vendedor_fkey; Type: FK CONSTRAINT; Schema: public; Owner: administrador
--

ALTER TABLE ONLY public.venta
    ADD CONSTRAINT venta_id_vendedor_fkey FOREIGN KEY (id_vendedor) REFERENCES public.usuarios(id);


-- Completed on 2026-11-11 19:22:18 UTC

--
-- PostgreSQL database dump complete
--

\unrestrict LdGqOPqkcUmWbJN7F1Dge7E9gWMf3MfNTmhGDmqEMRMVFIp8f5F1J1Oioi28hTq

