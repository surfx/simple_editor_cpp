#ifndef TRANSPARENCYDIALOG_H
#define TRANSPARENCYDIALOG_H

#include <QDialog>

class QLabel;
class QSlider;

class TransparencyDialog : public QDialog
{
    Q_OBJECT

signals:
    void transparencyChanged(int percent);

public:
    // "percent" is the amount of transparency: 0 = fully opaque, 90 = very transparent.
    explicit TransparencyDialog(int percent, QWidget *parent = nullptr);
    int transparencyPercent() const;
    void setTransparencyPercent(int percent);

private:
    void updateValueLabel(int percent);

    QSlider *slider;
    QLabel *valueLabel;
};

#endif // TRANSPARENCYDIALOG_H
