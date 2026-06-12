#!/bin/bash

set -e

echo "Actualizando npm..."
npm install -g npm@latest

echo "Instalando Gemini CLI..."
npm install -g @google/gemini-cli

echo "Verificando instalación..."
gemini --version || true

echo "Docker:"
docker --version

echo "Listo."
