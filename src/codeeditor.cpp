#include "codeeditor.h"
#include <QColor>
#include <QFont>

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
    QColor highlightColor = palette().color(QPalette::Highlight);
    highlightColor.setAlpha(40);
    setCaretLineBackgroundColor(highlightColor);

    // Line numbers
    setMarginType(1, QsciScintilla::NumberMargin);
    setMarginWidth(1, "0000");
    setMarginsBackgroundColor(QColor(Qt::lightGray));

    // COLUMN MODE (The magic part)
    // In Scintilla, rectangular selection is natively supported.
    // We enable multiple selection and typing into multiple selections.
    SendScintilla(QsciScintilla::SCI_SETMULTIPLESELECTION, true);
    SendScintilla(QsciScintilla::SCI_SETADDITIONALSELECTIONTYPING, true);
    
    // Explicitly set the rectangular selection modifier to Alt
    // SCI_SETSELECTIONMODE with SC_SEL_RECTANGULAR is for the current selection,
    // but to allow Alt+Mouse, we often need to ensure Scintilla knows the modifier.
    // By default Scintilla uses Alt, but some Linux Window Managers intercept Alt.
    // We can also try enabling virtual space for easier column editing.
    SendScintilla(QsciScintilla::SCI_SETVIRTUALSPACEOPTIONS, 1); // SCVS_RECTANGULARSELECTION
    
    // Tab settings
    setTabWidth(4);
    setIndentationsUseTabs(false); // Spaces for tabs is usually better for alignment
    setAutoIndent(true);

    // Wrap mode off by default
    setWrapMode(QsciScintilla::WrapNone);
}
