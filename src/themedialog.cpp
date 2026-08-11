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
            QColor("#c0caf5")  // braceForeground
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
            QColor("#cad3f5")
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
            QColor("#cdd6f4")
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
            QColor("#abb2bf")
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
            QColor("#88c0d0")
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
            QColor("#f1fa8c")
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
            QColor("#ebdbb2")
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
            QColor("#a6e22e")
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
            QColor("#268bd2")
        }
    };
    return themes;
}
