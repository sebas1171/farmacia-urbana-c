#!/bin/bash
echo "🚀 Iniciando despliegue automático de Farmacia Urbana..."

echo "📦 1. Levantando base de datos en Docker..."
sudo docker compose up -d

echo "🛠️ 2. Instalando dependencias de C y Python..."
sudo apt update > /dev/null 2>&1
sudo apt install -y gcc libpq-dev python3-pip python3-tk > /dev/null 2>&1
pip3 install customtkinter --break-system-packages > /dev/null 2>&1 || pip3 install customtkinter > /dev/null 2>&1

echo "⚙️ 3. Compilando el motor lógico en C..."
cd app
gcc -Wall -Wextra -O2 -o farmacia *.c -I/usr/include/postgresql -lpq -pthread

echo "✅ 4. Conectando e iniciando el sistema..."
export FARMACIA_DB_CONN='postgresql://administrador:password_seguro@127.0.0.1:5433/bd_farmacia'
python3 app_farmacia.py
