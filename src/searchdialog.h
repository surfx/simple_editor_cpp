#ifndef SEARCHDIALOG_H
#define SEARCHDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

class SearchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SearchDialog(QWidget *parent = nullptr, bool replaceMode = false);

    void setSearchText(const QString &text);
    QString getSearchText() const { return searchEdit->text(); }
    QString getReplaceText() const { return replaceEdit->text(); }
    bool isCaseSensitive() const { return caseCheckBox->isChecked(); }
    bool isWholeWord() const { return wordCheckBox->isChecked(); }
    bool isRegex() const { return regexCheckBox->isChecked(); }

signals:
    void findNext();
    void findPrevious();
    void replace();
    void replaceAll();

private:
    QLineEdit *searchEdit;
    QLineEdit *replaceEdit;
    QCheckBox *caseCheckBox;
    QCheckBox *wordCheckBox;
    QCheckBox *regexCheckBox;
    QPushButton *findNextButton;
    QPushButton *findPrevButton;
    QPushButton *replaceButton;
    QPushButton *replaceAllButton;
    QLabel *replaceLabel;
};

#endif // SEARCHDIALOG_H
