#include "themedialog.h"
#include <QLabel>
#include <QPushButton>

ThemeDialog::ThemeDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Selecionar Tema"));
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    layout->addWidget(new QLabel(tr("Escolha um tema para a aplicação:")));
    
    themeCombo = new QComboBox(this);
    for (const auto& theme : getAvailableThemes()) {
        themeCombo->addItem(theme.name);
    }
    layout->addWidget(themeCombo);

    connect(themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index){
        emit themeSelected(index);
    });
    
    QPushButton *closeButton = new QPushButton(tr("Fechar"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeButton);
    
    setLayout(layout);
}

QList<EditorTheme> ThemeDialog::getAvailableThemes()
{
    static QList<EditorTheme> themes = {
        {
            "Padrão (Tokyo Night)",
            QColor("#24283b"), // background
            QColor("#c5c6ca"), // foreground
            QColor("#c0caf5"), // caret
            QColor("#33467C"), // selectionBackground
            QColor("#c5c6ca"), // selectionForeground
            QColor("#2f334d"), // caretLine
            QColor("#24283b"), // marginsBackground
            QColor("#565f89"), // marginsForeground
            QColor("#3b4261"), // braceBackground
            QColor("#c0caf5"), // braceForeground
            QColor("#bb9af7"), // keyword
            QColor("#9ece6a"), // string
            QColor("#565f89"), // comment
            QColor("#2ac3de"), // type
            QColor("#bb9af7"), // preprocessor
            QColor("#ff9e64"), // number
            QColor("#f7768e"), // tag
            QColor("#e0af68")  // attribute
        },
        {
            "Cat Theme (Macchiato)",
            QColor("#24273a"),
            QColor("#cad3f5"),
            QColor("#f4dbd6"),
            QColor("#494d64"),
            QColor("#cad3f5"),
            QColor("#36394f"),
            QColor("#24273a"),
            QColor("#6e738d"),
            QColor("#5b6078"),
            QColor("#cad3f5"),
            QColor("#c6a0f6"), // keyword
            QColor("#a6da95"), // string
            QColor("#6e738d"), // comment
            QColor("#8aadf4"), // type
            QColor("#c6a0f6"), // preprocessor
            QColor("#f5a97f"), // number
            QColor("#ed8796"), // tag
            QColor("#eed49f")  // attribute
        },
        {
            "Mocha Theme",
            QColor("#1e1e2e"),
            QColor("#cdd6f4"),
            QColor("#f5e0dc"),
            QColor("#313244"),
            QColor("#cdd6f4"),
            QColor("#313244"),
            QColor("#1e1e2e"),
            QColor("#6c7086"),
            QColor("#45475a"),
            QColor("#cdd6f4"),
            QColor("#cba6f7"), // keyword
            QColor("#a6e3a1"), // string
            QColor("#6c7086"), // comment
            QColor("#89b4fa"), // type
            QColor("#cba6f7"), // preprocessor
            QColor("#fab387"), // number
            QColor("#f38ba8"), // tag
            QColor("#f9e2af")  // attribute
        },
        {
            "One Dark",
            QColor("#282c34"),
            QColor("#abb2bf"),
            QColor("#528bff"),
            QColor("#3e4451"),
            QColor("#abb2bf"),
            QColor("#2c313c"),
            QColor("#282c34"),
            QColor("#4b5263"),
            QColor("#3e4451"),
            QColor("#abb2bf"),
            QColor("#c678dd"), // keyword
            QColor("#98c379"), // string
            QColor("#5c6370"), // comment
            QColor("#e5c07b"), // type
            QColor("#c678dd"), // preprocessor
            QColor("#d19a66"), // number
            QColor("#e06c75"), // tag
            QColor("#61afef")  // attribute
        },
        {
            "Nord",
            QColor("#2e3440"),
            QColor("#d8dee9"),
            QColor("#d8dee9"),
            QColor("#434c5e"),
            QColor("#eceff4"),
            QColor("#3b4252"),
            QColor("#2e3440"),
            QColor("#4c566a"),
            QColor("#434c5e"),
            QColor("#88c0d0"),
            QColor("#81a1c1"), // keyword
            QColor("#a3be8c"), // string
            QColor("#4c566a"), // comment
            QColor("#8fbcbb"), // type
            QColor("#81a1c1"), // preprocessor
            QColor("#b48ead"), // number
            QColor("#81a1c1"), // tag
            QColor("#ebcb8b")  // attribute
        },
        {
            "Dracula",
            QColor("#282a36"),
            QColor("#f8f8f2"),
            QColor("#6272a4"),
            QColor("#44475a"),
            QColor("#f8f8f2"),
            QColor("#44475a"),
            QColor("#282a36"),
            QColor("#6272a4"),
            QColor("#44475a"),
            QColor("#f1fa8c"),
            QColor("#ff79c6"), // keyword
            QColor("#f1fa8c"), // string
            QColor("#6272a4"), // comment
            QColor("#8be9fd"), // type
            QColor("#ff79c6"), // preprocessor
            QColor("#bd93f9"), // number
            QColor("#ff79c6"), // tag
            QColor("#50fa7b")  // attribute
        },
        {
            "Gruvbox Dark",
            QColor("#282828"),
            QColor("#ebdbb2"),
            QColor("#a89984"),
            QColor("#3c3836"),
            QColor("#ebdbb2"),
            QColor("#3c3836"),
            QColor("#282828"),
            QColor("#928374"),
            QColor("#504945"),
            QColor("#ebdbb2"),
            QColor("#fb4934"), // keyword
            QColor("#b8bb26"), // string
            QColor("#928374"), // comment
            QColor("#fabd2f"), // type
            QColor("#8ec07c"), // preprocessor
            QColor("#d3869b"), // number
            QColor("#fb4934"), // tag
            QColor("#fe8019")  // attribute
        },
        {
            "Monokai",
            QColor("#272822"),
            QColor("#f8f8f2"),
            QColor("#f8f8f2"),
            QColor("#49483e"),
            QColor("#f8f8f2"),
            QColor("#3e3d32"),
            QColor("#272822"),
            QColor("#75715e"),
            QColor("#49483e"),
            QColor("#a6e22e"),
            QColor("#f92672"), // keyword
            QColor("#e6db74"), // string
            QColor("#75715e"), // comment
            QColor("#66d9ef"), // type
            QColor("#f92672"), // preprocessor
            QColor("#ae81ff"), // number
            QColor("#f92672"), // tag
            QColor("#a6e22e")  // attribute
        },
        {
            "Solarized Dark",
            QColor("#002b36"),
            QColor("#839496"),
            QColor("#93a1a1"),
            QColor("#073642"),
            QColor("#eee8d5"),
            QColor("#073642"),
            QColor("#002b36"),
            QColor("#586e75"),
            QColor("#073642"),
            QColor("#268bd2"),
            QColor("#859900"), // keyword
            QColor("#2aa198"), // string
            QColor("#586e75"), // comment
            QColor("#b58900"), // type
            QColor("#cb4b16"), // preprocessor
            QColor("#d33682"), // number
            QColor("#268bd2"), // tag
            QColor("#6c71c4")  // attribute
        }
    };
    return themes;
}
