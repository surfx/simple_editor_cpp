#include "codeeditor.h"
#include <QColor>
#include <QFont>
#include <QContextMenuEvent>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QProcess>
#include <QFileInfo>
#include <QDir>

CodeEditor::CodeEditor(QWidget *parent) : QsciScintilla(parent)
{
    // Basic settings
    setUtf8(true);
    
    // Apply Default Theme
    setTheme(ThemeDialog::getAvailableThemes().first());
    
    // Set Monospace font
    QFont font("Monospace", 10);
    font.setFixedPitch(true);
    setFont(font);

    // Current line highlight
    setCaretLineVisible(true);

    // Line numbers
    setMarginType(1, QsciScintilla::NumberMargin);
    setMarginWidth(1, "0000");
    
    // Brace matching
    // (Colors set by setTheme)

    // COLUMN MODE (The magic part)
    SendScintilla(QsciScintilla::SCI_SETMULTIPLESELECTION, true);
    SendScintilla(QsciScintilla::SCI_SETADDITIONALSELECTIONTYPING, true);
    SendScintilla(QsciScintilla::SCI_SETMULTIPASTE, true);
    SendScintilla(QsciScintilla::SCI_SETVIRTUALSPACEOPTIONS, 1); // SCVS_RECTANGULARSELECTION
    
    // Tab settings
    setTabWidth(4);
    setIndentationsUseTabs(false); // Spaces for tabs is usually better for alignment
    setAutoIndent(true);
    setTabIndents(true);
    setBackspaceUnindents(true);
    setIndentationGuides(true);

    // Wrap mode off by default
    setWrapMode(QsciScintilla::WrapNone);
}

void CodeEditor::setTheme(const EditorTheme &theme)
{
    setPaper(theme.background);
    setColor(theme.foreground);
    setCaretForegroundColor(theme.caret);
    setSelectionBackgroundColor(theme.selectionBackground);
    setSelectionForegroundColor(theme.selectionForeground);
    setCaretLineBackgroundColor(theme.caretLine);
    setMarginsBackgroundColor(theme.marginsBackground);
    setMarginsForegroundColor(theme.marginsForeground);
    setMatchedBraceBackgroundColor(theme.braceBackground);
    setMatchedBraceForegroundColor(theme.braceForeground);
}

void CodeEditor::duplicateLine()
{
    SendScintilla(QsciScintilla::SCI_LINEDUPLICATE);
}

void CodeEditor::deleteLine()
{
    SendScintilla(QsciScintilla::SCI_LINEDELETE);
}

void CodeEditor::moveLineUp()
{
    SendScintilla(QsciScintilla::SCI_MOVESELECTEDLINESUP);
}

void CodeEditor::moveLineDown()
{
    SendScintilla(QsciScintilla::SCI_MOVESELECTEDLINESDOWN);
}

void CodeEditor::indentSelection()
{
    SendScintilla(QsciScintilla::SCI_TAB);
}

void CodeEditor::unindentSelection()
{
    SendScintilla(QsciScintilla::SCI_BACKTAB);
}

void CodeEditor::toggleComment()
{
    int lineFrom, indexFrom, lineTo, indexTo;
    if (hasSelectedText()) {
        getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
        if (indexTo == 0 && lineTo > lineFrom) lineTo--;
    } else {
        getCursorPosition(&lineFrom, &indexFrom);
        lineTo = lineFrom;
    }

    beginUndoAction();
    for (int line = lineFrom; line <= lineTo; ++line) {
        QString lineText = text(line);
        if (lineText.trimmed().startsWith("//")) {
            int commentPos = lineText.indexOf("//");
            setSelection(line, commentPos, line, commentPos + 2);
            removeSelectedText();
        } else {
            insertAt("//", line, 0);
        }
    }
    endUndoAction();
}

void CodeEditor::keyPressEvent(QKeyEvent *e)
{
    // Handle navigation for multiple selections manually
    if (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right ||
        e->key() == Qt::Key_Up   || e->key() == Qt::Key_Down  ||
        e->key() == Qt::Key_Home || e->key() == Qt::Key_End) 
    {
        int selections = SendScintilla(SCI_GETSELECTIONS);
        if (selections > 1) {
            bool shift = e->modifiers() & Qt::ShiftModifier;
            bool ctrl = e->modifiers() & Qt::ControlModifier;

            for (int i = 0; i < selections; ++i) {
                int caret = SendScintilla(SCI_GETSELECTIONNCARET, i);
                int anchor = SendScintilla(SCI_GETSELECTIONNANCHOR, i);
                int newCaret = caret;

                switch (e->key()) {
                    case Qt::Key_Left:
                        if (ctrl) newCaret = SendScintilla(SCI_WORDLEFT);
                        else newCaret = SendScintilla(SCI_POSITIONRELATIVE, caret, -1);
                        break;
                    case Qt::Key_Right:
                        if (ctrl) newCaret = SendScintilla(SCI_WORDRIGHT);
                        else newCaret = SendScintilla(SCI_POSITIONRELATIVE, caret, 1);
                        break;
                    case Qt::Key_Up:
                        {
                            int line = SendScintilla(SCI_LINEFROMPOSITION, caret);
                            int col = SendScintilla(SCI_GETCOLUMN, caret);
                            if (line > 0) newCaret = SendScintilla(SCI_FINDCOLUMN, line - 1, col);
                        }
                        break;
                    case Qt::Key_Down:
                        {
                            int line = SendScintilla(SCI_LINEFROMPOSITION, caret);
                            int col = SendScintilla(SCI_GETCOLUMN, caret);
                            int lineCount = SendScintilla(SCI_GETLINECOUNT);
                            if (line < lineCount - 1) newCaret = SendScintilla(SCI_FINDCOLUMN, line + 1, col);
                        }
                        break;
                    case Qt::Key_Home:
                        newCaret = SendScintilla(SCI_POSITIONFROMLINE, SendScintilla(SCI_LINEFROMPOSITION, caret));
                        break;
                    case Qt::Key_End:
                        newCaret = SendScintilla(SCI_GETLINEENDPOSITION, SendScintilla(SCI_LINEFROMPOSITION, caret));
                        break;
                }

                SendScintilla(SCI_SETSELECTIONNCARET, i, newCaret);
                if (!shift) {
                    SendScintilla(SCI_SETSELECTIONNANCHOR, i, newCaret);
                }
            }
            e->accept();
            return;
        }
    }

    QsciScintilla::keyPressEvent(e);
}

void CodeEditor::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = new QMenu(this);

    QAction *undoAct = menu->addAction(tr("Desfazer"), this, &CodeEditor::undo);
    undoAct->setEnabled(isUndoAvailable());
    undoAct->setShortcut(QKeySequence::Undo);

    QAction *redoAct = menu->addAction(tr("Refazer"), this, &CodeEditor::redo);
    redoAct->setEnabled(isRedoAvailable());
    redoAct->setShortcut(QKeySequence::Redo);

    menu->addSeparator();

    QAction *cutAct = menu->addAction(tr("Recortar"), this, &CodeEditor::cut);
    cutAct->setEnabled(hasSelectedText());
    cutAct->setShortcut(QKeySequence::Cut);

    QAction *copyAct = menu->addAction(tr("Copiar"), this, &CodeEditor::copy);
    copyAct->setEnabled(hasSelectedText());
    copyAct->setShortcut(QKeySequence::Copy);

    menu->addAction(tr("Colar"), this, &CodeEditor::paste)->setShortcut(QKeySequence::Paste);
    menu->addAction(tr("Apagar"), this, &CodeEditor::removeSelectedText)->setEnabled(hasSelectedText());

    menu->addSeparator();

    QString filePath = property("filePath").toString();
    QAction *copyPathAct = menu->addAction(tr("Copiar Caminho"), [filePath]() {
        QApplication::clipboard()->setText(filePath);
    });
    copyPathAct->setEnabled(!filePath.isEmpty());

    QAction *openFolderAct = menu->addAction(tr("Abrir Pasta"), [filePath]() {
        if (!filePath.isEmpty()) {
            QString dir = QFileInfo(filePath).absolutePath();
            QProcess::startDetached("dolphin", QStringList() << dir);
        }
    });
    openFolderAct->setEnabled(!filePath.isEmpty());

    menu->addSeparator();

    menu->addAction(tr("Selecionar Tudo"), this, &CodeEditor::selectAll)->setShortcut(QKeySequence::SelectAll);

    menu->addSeparator();

    menu->addAction(tr("Duplicar Linha"), this, &CodeEditor::duplicateLine)->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    menu->addAction(tr("Apagar Linha"), this, &CodeEditor::deleteLine)->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
    menu->addAction(tr("Comentar/Descomentar"), this, &CodeEditor::toggleComment)->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));

    menu->exec(event->globalPos());
    delete menu;
}
