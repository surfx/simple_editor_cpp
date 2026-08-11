# Simple C++ Editor

![Simple C++ Editor](assets/simple_editor.png)

Um editor de texto leve e funcional inspirado no Notepad++, desenvolvido em C++ com o framework Qt6 e o motor Scintilla. Projetado para ser rápido, intuitivo e resiliente.

## Funcionalidades Principais

- **Múltiplas Abas:** Suporte para abrir vários arquivos simultaneamente com gerenciamento dinâmico.
- **Sincronização com o Disco (Live Sync):**
    - **Detecção de Mudanças Externas:** O editor monitora se os arquivos abertos foram alterados por outros programas (VSCode, Gedit, etc.) e exibe uma barra de notificação amarela permitindo o recarregamento imediato.
    - **Resiliência a Deleções:** Se um arquivo for deletado do disco enquanto estiver aberto, o editor marca a aba com um asterisco (`*`) e mantém o conteúdo na memória, protegendo seus dados.
    - **Salvamento Seguro:** Ao tentar salvar um arquivo que foi deletado externamente, o editor redireciona automaticamente para a função "Salvar Como", evitando erros de gravação.
- **Persistência de Sessão Inteligente:**
    - Ao fechar o editor, ele lembra quais abas estavam abertas.
    - Restaura o conteúdo exato de cada aba, inclusive rascunhos voláteis ou arquivos que foram deletados do disco entre as sessões, sem disparar alertas desnecessários no reinício.
- **Edição Avançada:**
    - **Modo Coluna (Seleção em Bloco):** Segure `Alt` e arraste o mouse ou use `Alt+Shift+Setas` para editar texto verticalmente.
    - **Manipulação de Linhas:** Atalhos para duplicar, apagar e mover linhas para cima/baixo (`Alt+Up/Down`).
    - **Comentários Rápidos:** `Ctrl+Q` para comentar/descomentar blocos de código.
- **Interface e Temas:**
    - **Destaque de Sintaxe:** Suporte completo para C++ e linguagens comuns via Scintilla.
    - **Temas Customizáveis:** Interface baseada no tema *Tokyo Night* com suporte a troca de temas via diálogo.
    - **Interface Minimalista:** Barra de ferramentas otimizada para produtividade.
- **Integração com o Sistema:**
    - **Instância Única:** Se você tentar abrir um novo arquivo e o editor já estiver rodando, ele abrirá o arquivo em uma nova aba na janela existente.
    - **Drag and Drop:** Suporte para arrastar arquivos do gerenciador de pastas diretamente para o editor.

## Atalhos Úteis

- `Ctrl + N`: Novo arquivo
- `Ctrl + O`: Abrir arquivo
- `Ctrl + S`: Salvar (ou Salvar Como se o arquivo original não existir)
- `Ctrl + Q`: Comentar/Descomentar
- `Ctrl + D`: Duplicar linha
- `Ctrl + F`: Localizar
- `Ctrl + H`: Substituir
- `Alt + Up/Down`: Mover linha
- `Ctrl + Shift + T`: Reabrir última aba fechada

## Pré-requisitos

Para compilar e rodar este projeto, você precisa instalar o Qt6 e a biblioteca QScintilla2 para Qt6. No **Arch Linux**, execute:

```bash
sudo pacman -S qt6-base qscintilla-qt6 cmake base-devel
```

## Como Buildar e Rodar

O projeto inclui scripts facilitadores na pasta `scripts/`:

1. **Compilar:**
   ```bash
   ./scripts/build.sh
   ```
2. **Executar:**
   ```bash
   ./scripts/run.sh
   ```

## Instalação no Sistema

Para integrar o editor ao seu ambiente desktop (ícones e menu "Abrir com"):

```bash
sudo ./scripts/install.sh
```
