#!/bin/bash
# scripts/install.sh

# Verifica se é root
if [ "$EUID" -ne 0 ]; then
  echo "Por favor, execute como root (sudo ./scripts/install.sh)"
  exit 1
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# 1. Garante que o projeto está buildado
echo "Compilando o projeto..."
mkdir -p "$PROJECT_ROOT/build"
cd "$PROJECT_ROOT/build"
cmake ..
cmake --build .

if [ $? -ne 0 ]; then
    echo "Erro na compilação. Instalação abortada."
    exit 1
fi

# 2. Instala o binário
echo "Instalando binário em /usr/local/bin..."
cp "$PROJECT_ROOT/build/SimpleEditor" /usr/local/bin/

# 3. Instala o arquivo .desktop
echo "Instalando entrada de menu em /usr/share/applications..."
cp "$PROJECT_ROOT/SimpleEditor.desktop" /usr/share/applications/

# 4. Atualiza banco de dados de MIME e desktop (opcional mas recomendado)
update-desktop-database /usr/share/applications/ 2>/dev/null

echo "--------------------------------------------------"
echo "Instalação concluída com sucesso!"
echo "Agora você pode encontrar o 'Simple Editor' no seu menu de aplicativos"
echo "ou clicar com o botão direito em um arquivo e escolher 'Abrir com...'."
echo "--------------------------------------------------"
