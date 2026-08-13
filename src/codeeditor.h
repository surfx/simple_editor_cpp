#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <Qsci/qsciscintilla.h>
#include "themedialog.h"

class CodeEditor : public QsciScintilla
{
    Q_OBJECT

public:
    CodeEditor(QWidget *parent = nullptr);

    bool isModified() const { return QsciScintilla::isModified(); }
    void setModified(bool m) { QsciScintilla::setModified(m); }

    void setTheme(const EditorTheme &theme);

    // Text manipulation
    void duplicateLine();
    void deleteLine();
    void moveLineUp();
    void moveLineDown();
    void toggleComment();
    void indentSelection();
    void unindentSelection();

private slots:
    void highlightSelections();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *e) override;
};

#endif // CODEEDITOR_H
