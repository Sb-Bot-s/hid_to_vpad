#!/bin/bash

set -e

echo "Instalando herramientas de desarrollo (C++, clang-format)..."
sudo apt-get update && sudo apt-get install -y clang-format build-essential

echo "Actualizando npm..."
npm install -g npm@latest

echo "Instalando Gemini CLI..."
npm install -g @google/gemini-cli

echo "Verificando instalación..."
gemini --version || true

echo "Docker:"
docker --version

echo "Listo."
