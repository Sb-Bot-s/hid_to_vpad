#!/bin/bash
# Script para replicar los pasos del CI de manera local en Codespaces.
# Esto permite validar formato y compilación sin depender de GitHub Actions.

set -e

echo "=== [1/3] Aplicando formato (Clang-Format) ==="
clang-format -i src/*.cpp src/*.h tools/*.py
echo "Formato aplicado correctamente."

echo "=== [2/3] Generando src/version.h ==="
git_hash=$(git rev-parse --short HEAD)
cat <<EOF > ./src/version.h
#pragma once
#define VERSION_EXTRA " (local-$git_hash)"
EOF
echo "src/version.h generado con el hash: $git_hash"

echo "=== [3/3] Compilando el plugin (Docker) ==="
# Construir la imagen de compilación si no existe
docker build . -t hid-to-vpad-plugin-builder

# Ejecutar el build
docker run --rm -v "${PWD}:/project" hid-to-vpad-plugin-builder make

echo ""
echo "=== PROCESO COMPLETADO ==="
echo "Si no hubo errores arriba, el archivo 'hidtovpad.wps' está listo."
