#ifndef THEMEDIALOG_H
#define THEMEDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QColor>
#include <QString>
#include <QList>

struct EditorTheme {
    QString name;
    QColor background;
    QColor foreground;
    QColor caret;
    QColor selectionBackground;
    QColor selectionForeground;
    QColor caretLine;
    QColor marginsBackground;
    QColor marginsForeground;
    QColor braceBackground;
    QColor braceForeground;
    
    // Syntax colors
    QColor keyword;
    QColor string;
    QColor comment;
    QColor type;
    QColor preprocessor;
    QColor number;
    QColor tag;
    QColor attribute;
};

class ThemeDialog : public QDialog
{
    Q_OBJECT

signals:
    void themeSelected(int index);

public:
    ThemeDialog(QWidget *parent = nullptr);
    static QList<EditorTheme> getAvailableThemes();
    int getSelectedThemeIndex() const { return themeCombo->currentIndex(); }
    void setSelectedThemeIndex(int index) { themeCombo->setCurrentIndex(index); }

private:
    QComboBox *themeCombo;
};

#endif // THEMEDIALOG_H
