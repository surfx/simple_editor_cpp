#include "transparencydialog.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace {
// The window is never made completely invisible: at 90% transparency there is
// still enough left to see and interact with the editor (and the dialog itself
// stays fully opaque, so the value can always be changed back).
constexpr int kMaxTransparency = 90;
}

TransparencyDialog::TransparencyDialog(int percent, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Transparência"));

    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(tr("Escolha o nível de transparência da janela:"), this));

    slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(0, kMaxTransparency);
    slider->setSingleStep(1);
    slider->setPageStep(10);
    slider->setTickPosition(QSlider::TicksBelow);
    slider->setTickInterval(10);
    slider->setValue(qBound(0, percent, kMaxTransparency));
    layout->addWidget(slider);

    valueLabel = new QLabel(this);
    layout->addWidget(valueLabel);
    updateValueLabel(slider->value());

    QHBoxLayout *buttonLayout = new QHBoxLayout;

    QPushButton *resetButton = new QPushButton(tr("Restaurar (opaca)"), this);
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        slider->setValue(0);
    });
    buttonLayout->addWidget(resetButton);

    buttonLayout->addStretch();

    QPushButton *closeButton = new QPushButton(tr("Fechar"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeButton);

    layout->addLayout(buttonLayout);

    connect(slider, &QSlider::valueChanged, this, [this](int value) {
        updateValueLabel(value);
        emit transparencyChanged(value);
    });

    resize(360, sizeHint().height());
}

int TransparencyDialog::transparencyPercent() const
{
    return slider->value();
}

void TransparencyDialog::setTransparencyPercent(int percent)
{
    slider->setValue(qBound(0, percent, kMaxTransparency));
}

void TransparencyDialog::updateValueLabel(int percent)
{
    if (percent == 0) {
        valueLabel->setText(tr("0% — janela totalmente opaca"));
    } else {
        valueLabel->setText(tr("%1% de transparência").arg(percent));
    }
}
