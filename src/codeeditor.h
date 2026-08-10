#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <Qsci/qsciscintilla.h>

class CodeEditor : public QsciScintilla
{
    Q_OBJECT

public:
    CodeEditor(QWidget *parent = nullptr);

    bool isModified() const { return QsciScintilla::isModified(); }
    void setModified(bool m) { QsciScintilla::setModified(m); }
};

#endif // CODEEDITOR_H
