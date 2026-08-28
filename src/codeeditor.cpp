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

// Lexer Includes
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexerhtml.h>
#include <Qsci/qscilexercss.h>
#include <Qsci/qscilexerjavascript.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexerxml.h>
#include <Qsci/qscilexerbash.h>
#include <Qsci/qscilexersql.h>
#include <Qsci/qscilexerjson.h>

CodeEditor::CodeEditor(QWidget *parent) : QsciScintilla(parent)
{
    // Basic settings
    setUtf8(true);
    
    // Set Monospace font
    QFont font("Monospace", 10);
    font.setFixedPitch(true);
    setFont(font);

    // Current line highlight
    setCaretLineVisible(true);

    // Line numbers
    setMarginType(1, QsciScintilla::NumberMargin);
    setMarginWidth(1, "0000");
    
    // Folding margin (initially 0, will be enabled by setLanguage)
    setFolding(QsciScintilla::CircledTreeFoldStyle);
    setMarginType(2, QsciScintilla::SymbolMargin);
    setMarginWidth(2, 0);
    setMarginSensitivity(2, false);
    
    // Apply Default Theme
    setTheme(ThemeDialog::getAvailableThemes().first());
    
    // Brace matching
    setBraceMatching(QsciScintilla::SloppyBraceMatch);

    // Tab settings
    setTabWidth(4);
    setIndentationsUseTabs(false);
    setAutoIndent(false);
    setTabIndents(true);
    setBackspaceUnindents(true);
    setIndentationGuides(true);

    // Wrap mode off by default
    setWrapMode(QsciScintilla::WrapNone);

    // Setup Highlight Indicator (Indicator 8)
    // Using StraightBoxIndicator for a solid, modern look
    indicatorDefine(QsciScintilla::StraightBoxIndicator, 8);
    setIndicatorForegroundColor(QColor(0, 0, 255, 120), 8); // Slightly more opaque
    setIndicatorOutlineColor(QColor(0, 0, 255, 255), 8); // Solid outline
    setIndicatorDrawUnder(true, 8); // Keep it behind text for legibility

    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightSelections);
}

void CodeEditor::highlightSelections()
{
    // Recursion guard
    static bool isHighlighting = false;
    if (isHighlighting) return;
    isHighlighting = true;

    // Use Scintilla messages to find and highlight without changing selection.
    long docLen = SendScintilla(SCI_GETLENGTH);
    
    // Clear previous indicators (Indicator 8)
    SendScintilla(SCI_SETINDICATORCURRENT, 8);
    SendScintilla(SCI_INDICATORCLEARRANGE, 0, docLen);

    // Skip the full-document scan on very large files: an O(n) search on every
    // cursor move would noticeably lag typing/navigation.
    const long kMaxDocLenForHighlight = 2 * 1024 * 1024; // 2 MB
    if (docLen > kMaxDocLenForHighlight) {
        isHighlighting = false;
        return;
    }

    if (hasSelectedText()) {
        QString selected = selectedText();
        // Only highlight if selection is a reasonable word/phrase
        if (!selected.isEmpty() && selected.length() >= 2 && selected.length() <= 100) {
            QByteArray bytes = selected.toUtf8();
            
            // Set search parameters (SCFIND_MATCHCASE | SCFIND_WHOLEWORD if needed)
            // For now, let's match what the user selects exactly.
            SendScintilla(SCI_SETSEARCHFLAGS, SCFIND_MATCHCASE);
            
            long searchStart = 0;
            while (searchStart < docLen) {
                SendScintilla(SCI_SETTARGETSTART, searchStart);
                SendScintilla(SCI_SETTARGETEND, docLen);
                
                long pos = SendScintilla(SCI_SEARCHINTARGET, bytes.length(), bytes.constData());
                if (pos == -1) break;
                
                long targetStart = SendScintilla(SCI_GETTARGETSTART);
                long targetEnd = SendScintilla(SCI_GETTARGETEND);
                
                // Fill indicator for this range
                SendScintilla(SCI_SETINDICATORCURRENT, 8);
                SendScintilla(SCI_INDICATORFILLRANGE, targetStart, targetEnd - targetStart);
                
                searchStart = targetEnd;
                if (searchStart >= docLen) break;
            }
        }
    }

    isHighlighting = false;
}

void CodeEditor::setTheme(const EditorTheme &theme)
{
    currentTheme = theme;
    
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
    
    // Folding colors
    setFoldMarginColors(theme.marginsBackground, theme.marginsBackground);
    
    // IMPORTANT: Re-assert global font to prevent lexers from changing sizes/families
    QFont globalFont("Monospace", 10);
    globalFont.setFixedPitch(true);
    setFont(globalFont);

    if (lexer()) {
        lexer()->setFont(globalFont);
        applyThemeToLexer(lexer(), theme);
    }

    // Update indicator color based on theme
    // We use a higher alpha (160) for the fill and full opacity for the border to make it POP
    setIndicatorForegroundColor(QColor(theme.selectionBackground.red(), 
                                       theme.selectionBackground.green(), 
                                       theme.selectionBackground.blue(), 160), 8);
    setIndicatorOutlineColor(theme.selectionBackground, 8);
}

void CodeEditor::applyThemeToLexer(QsciLexer *l, const EditorTheme &theme)
{
    if (!l) return;

    l->setPaper(theme.background);
    l->setDefaultPaper(theme.background);
    l->setColor(theme.foreground);
    l->setDefaultColor(theme.foreground);

    // Common styles mapping for supported lexers using qobject_cast for safety
    if (auto *cpp = qobject_cast<QsciLexerCPP *>(l)) {
        cpp->setColor(theme.keyword, QsciLexerCPP::Keyword);
        cpp->setColor(theme.type, QsciLexerCPP::KeywordSet2);
        cpp->setColor(theme.string, QsciLexerCPP::DoubleQuotedString);
        cpp->setColor(theme.string, QsciLexerCPP::SingleQuotedString);
        cpp->setColor(theme.comment, QsciLexerCPP::Comment);
        cpp->setColor(theme.comment, QsciLexerCPP::CommentLine);
        cpp->setColor(theme.preprocessor, QsciLexerCPP::PreProcessor);
        cpp->setColor(theme.number, QsciLexerCPP::Number);
    } else if (auto *html = qobject_cast<QsciLexerHTML *>(l)) {
        html->setColor(theme.tag, QsciLexerHTML::Tag);
        html->setColor(theme.attribute, QsciLexerHTML::Attribute);
        html->setColor(theme.string, QsciLexerHTML::HTMLDoubleQuotedString);
        html->setColor(theme.string, QsciLexerHTML::HTMLSingleQuotedString);
        html->setColor(theme.comment, QsciLexerHTML::HTMLComment);
    } else if (auto *css = qobject_cast<QsciLexerCSS *>(l)) {
        css->setColor(theme.tag, QsciLexerCSS::Tag);
        css->setColor(theme.keyword, QsciLexerCSS::CSS1Property);
        css->setColor(theme.keyword, QsciLexerCSS::CSS2Property);
        css->setColor(theme.string, QsciLexerCSS::DoubleQuotedString);
        css->setColor(theme.string, QsciLexerCSS::SingleQuotedString);
        css->setColor(theme.comment, QsciLexerCSS::Comment);
    } else if (auto *py = qobject_cast<QsciLexerPython *>(l)) {
        py->setColor(theme.keyword, QsciLexerPython::Keyword);
        py->setColor(theme.string, QsciLexerPython::DoubleQuotedString);
        py->setColor(theme.string, QsciLexerPython::SingleQuotedString);
        py->setColor(theme.comment, QsciLexerPython::Comment);
        py->setColor(theme.number, QsciLexerPython::Number);
        py->setColor(theme.type, QsciLexerPython::ClassName);
    } else if (auto *bash = qobject_cast<QsciLexerBash *>(l)) {
        bash->setColor(theme.keyword, QsciLexerBash::Keyword);
        bash->setColor(theme.string, QsciLexerBash::DoubleQuotedString);
        bash->setColor(theme.string, QsciLexerBash::SingleQuotedString);
        bash->setColor(theme.comment, QsciLexerBash::Comment);
        bash->setColor(theme.number, QsciLexerBash::Number);
    } else if (auto *sql = qobject_cast<QsciLexerSQL *>(l)) {
        sql->setColor(theme.keyword, QsciLexerSQL::Keyword);
        sql->setColor(theme.string, QsciLexerSQL::DoubleQuotedString);
        sql->setColor(theme.string, QsciLexerSQL::SingleQuotedString);
        sql->setColor(theme.comment, QsciLexerSQL::Comment);
        sql->setColor(theme.comment, QsciLexerSQL::CommentLine);
        sql->setColor(theme.number, QsciLexerSQL::Number);
    } else if (auto *json = qobject_cast<QsciLexerJSON *>(l)) {
        json->setColor(theme.keyword, QsciLexerJSON::Property);
        json->setColor(theme.string, QsciLexerJSON::String);
        json->setColor(theme.number, QsciLexerJSON::Number);
        json->setColor(theme.keyword, QsciLexerJSON::Keyword);
    }
}

void CodeEditor::setLanguage(const QString &lang)
{
    m_language = lang;
    // Keep a handle to the previous lexer so it can be freed after being
    // replaced; QsciScintilla::setLexer() does not delete the old one, which
    // otherwise leaks a lexer object every time the language is switched.
    QsciLexer *oldLexer = lexer();
    QsciLexer *l = nullptr;
    QString lcase = lang.toLower();

    // Reset margin for non-folding states
    setMarginWidth(2, 0);
    setMarginSensitivity(2, false);

    if (lcase == "cpp" || lcase == "c++" || lcase == "c") {
        l = new QsciLexerCPP(this);
    } else if (lcase == "html") {
        l = new QsciLexerHTML(this);
    } else if (lcase == "css") {
        l = new QsciLexerCSS(this);
    } else if (lcase == "javascript" || lcase == "js") {
        l = new QsciLexerJavaScript(this);
    } else if (lcase == "json") {
        l = new QsciLexerJSON(this);
    } else if (lcase == "python" || lcase == "py") {
        l = new QsciLexerPython(this);
    } else if (lcase == "xml") {
        l = new QsciLexerXML(this);
    } else if (lcase == "bash" || lcase == "sh") {
        l = new QsciLexerBash(this);
    } else if (lcase == "sql") {
        l = new QsciLexerSQL(this);
    }

    setLexer(l);

    if (oldLexer && oldLexer != l) {
        delete oldLexer;
    }

    if (l) {
        setMarginWidth(2, 16);
        setMarginSensitivity(2, true);
        l->setFont(font());
        applyThemeToLexer(l, currentTheme);
        
        if (lcase == "html") {
            SendScintilla(QsciScintilla::SCI_SETPROPERTY, "asp.default.language", "1");
            SendScintilla(QsciScintilla::SCI_SETPROPERTY, "html.tags.case.sensitive", "0");
        }
    }

    // Force re-application of theme colors to the whole editor to fix background issues
    setTheme(currentTheme);
    
    // RE-ASSERT tab, font and auto-indent settings which can be reset by lexers
    setTabWidth(4);
    setIndentationsUseTabs(false);
    setAutoIndent(false);
    
    // Clear folding if no lexer
    if (!l) {
        SendScintilla(QsciScintilla::SCI_SETFOLDLEVEL, 0, 0x400);
        // Scintilla reset styling
        SendScintilla(QsciScintilla::SCI_CLEARDOCUMENTSTYLE);
    }
}

void CodeEditor::detectLexer(const QString &filePath)
{
    QString ext = QFileInfo(filePath).suffix().toLower();
    
    if (ext == "cpp" || ext == "h" || ext == "c" || ext == "hpp" || ext == "cc") {
        setLanguage("cpp");
    } else if (ext == "html" || ext == "htm") {
        setLanguage("html");
    } else if (ext == "css") {
        setLanguage("css");
    } else if (ext == "js") {
        setLanguage("javascript");
    } else if (ext == "json") {
        setLanguage("json");
    } else if (ext == "py") {
        setLanguage("python");
    } else if (ext == "xml") {
        setLanguage("xml");
    } else if (ext == "sh") {
        setLanguage("bash");
    } else if (ext == "sql") {
        setLanguage("sql");
    } else {
        setLexer(nullptr);
    }
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
    // Pick the line-comment token based on the current language so this works
    // correctly for Python/Bash (#) and SQL (--), not just C-style languages.
    QString lcase = m_language.toLower();
    QString marker = "//";
    if (lcase == "python" || lcase == "py" || lcase == "bash" || lcase == "sh") {
        marker = "#";
    } else if (lcase == "sql") {
        marker = "--";
    }

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
        if (lineText.trimmed().startsWith(marker)) {
            int commentPos = lineText.indexOf(marker);
            setSelection(line, commentPos, line, commentPos + marker.length());
            removeSelectedText();
        } else {
            insertAt(marker, line, 0);
        }
    }
    endUndoAction();
}

void CodeEditor::keyPressEvent(QKeyEvent *e)
{
    // Custom Auto-Indent implementation to fix the "look-back" bug.
    // We handle this manually to ensure it strictly follows the current line's indentation.
    Qt::KeyboardModifiers mods = e->modifiers();
    mods &= ~Qt::KeypadModifier; // Ignore if it's from the keypad

    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && mods == Qt::NoModifier) {
        int selections = SendScintilla(SCI_GETSELECTIONS);
        if (selections <= 1) {
            beginUndoAction();

            // 1. Handle selection removal if any
            if (hasSelectedText()) {
                removeSelectedText();
            }

            // 2. Get current line indentation BEFORE inserting newline
            int line, col;
            getCursorPosition(&line, &col);
            int indent = SendScintilla(SCI_GETLINEINDENTATION, line);

            // 3. Insert newline manually (bypassing base class "smart" logic)
            SendScintilla(SCI_NEWLINE);

            // 4. Apply indentation to the new line
            int newLine, newCol;
            getCursorPosition(&newLine, &newCol);
            
            if (newLine > line) {
                SendScintilla(SCI_SETLINEINDENTATION, newLine, indent);
                
                // Move the cursor to the end of the new indentation
                int indentPos = SendScintilla(SCI_GETLINEINDENTPOSITION, newLine);
                int lineStartPos = SendScintilla(SCI_POSITIONFROMLINE, newLine);
                setCursorPosition(newLine, indentPos - lineStartPos);
            }

            endUndoAction();
            e->accept();
            return;
        }
    }

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

void CodeEditor::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        // Let the drop bubble up to MainWindow so files can be opened as tabs,
        // regardless of where over the editor the user drops them.
        event->ignore();
        return;
    }
    QsciScintilla::dragEnterEvent(event);
}

void CodeEditor::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }
    QsciScintilla::dragMoveEvent(event);
}

void CodeEditor::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }
    QsciScintilla::dropEvent(event);
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
