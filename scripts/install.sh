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

# 2. Instala o binário e o atalho curto em /usr/local/bin
echo "Instalando binário em /usr/local/bin..."
cp "$PROJECT_ROOT/build/SimpleEditor" /usr/local/bin/SimpleEditor
chmod +x /usr/local/bin/SimpleEditor

# Cria um comando curto 'editor' apontando para o SimpleEditor
ln -sf /usr/local/bin/SimpleEditor /usr/local/bin/editor

# 3. Instala o arquivo .desktop
echo "Instalando entrada de menu em /usr/share/applications..."
cp "$PROJECT_ROOT/SimpleEditor.desktop" /usr/share/applications/

# 4. Atualiza banco de dados de MIME e desktop
echo "Atualizando caches do sistema..."
update-desktop-database /usr/share/applications/ 2>/dev/null
update-mime-database /usr/share/mime 2>/dev/null
# Atualiza o cache do KDE (Plasma), para o ícone/atalho ser reconhecido na hora
if command -v kbuildsycoca6 >/dev/null 2>&1; then
    kbuildsycoca6 --noincremental >/dev/null 2>&1
elif command -v kbuildsycoca5 >/dev/null 2>&1; then
    kbuildsycoca5 --noincremental >/dev/null 2>&1
fi

echo "--------------------------------------------------"
echo "Instalação concluída com sucesso!"
echo "Use 'SimpleEditor' ou 'editor arquivo.txt' no terminal."
echo ""
echo "IMPORTANTE (ícone na barra de tarefas):"
echo "Se você fixou o executável na barra (atalho do tipo"
echo "file:///usr/local/bin/editor), ele aparece como '?' quando o editor"
echo "está fechado. Remova esse atalho fixado e fixe o aplicativo pelo menu:"
echo "  Menu de aplicativos > Simple Editor > botão direito > Fixar na barra."
echo "--------------------------------------------------"
