#include "searchdialog.h"
#include <QGridLayout>

SearchDialog::SearchDialog(QWidget *parent, bool replaceMode)
    : QDialog(parent)
{
    setWindowTitle(replaceMode ? tr("Localizar e Substituir") : tr("Localizar"));
    
    QGridLayout *layout = new QGridLayout(this);

    layout->addWidget(new QLabel(tr("Localizar:")), 0, 0);
    searchEdit = new QLineEdit(this);
    layout->addWidget(searchEdit, 0, 1, 1, 2);

    replaceLabel = new QLabel(tr("Substituir por:"), this);
    replaceEdit = new QLineEdit(this);
    layout->addWidget(replaceLabel, 1, 0);
    layout->addWidget(replaceEdit, 1, 1, 1, 2);

    if (!replaceMode) {
        replaceLabel->hide();
        replaceEdit->hide();
    }

    caseCheckBox = new QCheckBox(tr("Diferenciar maiúsculas/minúsculas"), this);
    wordCheckBox = new QCheckBox(tr("Palavras inteiras"), this);
    regexCheckBox = new QCheckBox(tr("Expressões regulares"), this);

    layout->addWidget(caseCheckBox, 2, 0, 1, 3);
    layout->addWidget(wordCheckBox, 3, 0, 1, 3);
    layout->addWidget(regexCheckBox, 4, 0, 1, 3);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    findPrevButton = new QPushButton(tr("Anterior"), this);
    findNextButton = new QPushButton(tr("Próximo"), this);
    buttonLayout->addWidget(findPrevButton);
    buttonLayout->addWidget(findNextButton);
    
    if (replaceMode) {
        replaceButton = new QPushButton(tr("Substituir"), this);
        replaceAllButton = new QPushButton(tr("Substituir Tudo"), this);
        buttonLayout->addWidget(replaceButton);
        buttonLayout->addWidget(replaceAllButton);
        
        connect(replaceButton, &QPushButton::clicked, this, &SearchDialog::replace);
        connect(replaceAllButton, &QPushButton::clicked, this, &SearchDialog::replaceAll);
    }

    layout->addLayout(buttonLayout, 5, 0, 1, 3);

    connect(findNextButton, &QPushButton::clicked, this, &SearchDialog::findNext);
    connect(findPrevButton, &QPushButton::clicked, this, &SearchDialog::findPrevious);
    
    searchEdit->setFocus();
    
    // Standard dialog behavior
    findNextButton->setDefault(true);
}
