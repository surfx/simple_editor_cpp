#include "mainwindow.h"
#include "sessionmanager.h"
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QCloseEvent>
#include <QPushButton>
#include <QToolBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    tabWidget = new QTabWidget(this);
    tabWidget->setTabsClosable(true);
    tabWidget->setMovable(true);
    tabWidget->setAttribute(Qt::WA_AcceptDrops, false);

    setCentralWidget(tabWidget);

    connect(tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    createMenus();
    createToolBar();
    loadSession();

    if (tabWidget->count() == 0) {
        newFile();
    }

    setWindowTitle("Simple C++ Editor");
    setWindowIcon(QIcon::fromTheme("text-x-generic"));
    resize(800, 600);
    setAcceptDrops(true);

    // Setup Single Instance Server
    localServer = new QLocalServer(this);
    QLocalServer::removeServer("SimpleEditorServer");
    if (localServer->listen("SimpleEditorServer")) {
        connect(localServer, &QLocalServer::newConnection, this, &MainWindow::newLocalConnection);
    }
}

MainWindow::~MainWindow()
{
}

void MainWindow::newLocalConnection()
{
    QLocalSocket *socket = localServer->nextPendingConnection();
    if (socket) {
        connect(socket, &QLocalSocket::readyRead, [this, socket]() {
            QString message = QString::fromUtf8(socket->readAll());
            handleMessage(message);
            socket->disconnectFromServer();
        });
    }
}

void MainWindow::handleMessage(const QString &message)
{
    if (!message.isEmpty()) {
        addEditorTab(message);
    }
    
    // Bring window to front
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        for (const QUrl &url : urlList) {
            QString filePath = url.toLocalFile();
            if (!filePath.isEmpty()) {
                addEditorTab(filePath);
            }
        }
        event->acceptProposedAction();
    }
}

void MainWindow::createToolBar()
{
    QToolBar *mainToolBar = addToolBar(tr("Principal"));
    mainToolBar->setMovable(false);
    mainToolBar->setFloatable(false);
    mainToolBar->setIconSize(QSize(24, 24));

    // New Tab
    QAction *newAct = new QAction(QIcon::fromTheme("document-new"), tr("Nova Aba"), this);
    newAct->setShortcut(QKeySequence::New);
    newAct->setToolTip(tr("Nova Aba (Ctrl+N)"));
    connect(newAct, &QAction::triggered, this, &MainWindow::newFile);
    mainToolBar->addAction(newAct);

    // Open
    QAction *openAct = new QAction(QIcon::fromTheme("folder-open"), tr("Abrir"), this);
    openAct->setShortcut(QKeySequence::Open);
    openAct->setToolTip(tr("Abrir Arquivo (Ctrl+O)"));
    connect(openAct, &QAction::triggered, this, &MainWindow::openFile);
    mainToolBar->addAction(openAct);

    // Save
    QAction *saveAct = new QAction(QIcon::fromTheme("media-floppy"), tr("Salvar"), this);
    saveAct->setShortcut(QKeySequence::Save);
    saveAct->setToolTip(tr("Salvar (Ctrl+S)"));
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveFile);
    mainToolBar->addAction(saveAct);

    mainToolBar->addSeparator();

    // Standard Edit Actions in Toolbar
    QAction *undoAct = new QAction(QIcon::fromTheme("edit-undo"), tr("Desfazer"), this);
    undoAct->setShortcut(QKeySequence::Undo);
    undoAct->setToolTip(tr("Desfazer (Ctrl+Z)"));
    connect(undoAct, &QAction::triggered, [this](){ if(currentEditor()) currentEditor()->undo(); });
    mainToolBar->addAction(undoAct);

    QAction *redoAct = new QAction(QIcon::fromTheme("edit-redo"), tr("Refazer"), this);
    redoAct->setShortcut(QKeySequence::Redo);
    redoAct->setToolTip(tr("Refazer (Ctrl+Y)"));
    connect(redoAct, &QAction::triggered, [this](){ if(currentEditor()) currentEditor()->redo(); });
    mainToolBar->addAction(redoAct);

    mainToolBar->addSeparator();

    QAction *cutAct = new QAction(QIcon::fromTheme("edit-cut"), tr("Recortar"), this);
    cutAct->setShortcut(QKeySequence::Cut);
    cutAct->setToolTip(tr("Recortar (Ctrl+X)"));
    connect(cutAct, &QAction::triggered, [this](){ if(currentEditor()) currentEditor()->cut(); });
    mainToolBar->addAction(cutAct);

    QAction *copyAct = new QAction(QIcon::fromTheme("edit-copy"), tr("Copiar"), this);
    copyAct->setShortcut(QKeySequence::Copy);
    copyAct->setToolTip(tr("Copiar (Ctrl+C)"));
    connect(copyAct, &QAction::triggered, [this](){ if(currentEditor()) currentEditor()->copy(); });
    mainToolBar->addAction(copyAct);

    QAction *pasteAct = new QAction(QIcon::fromTheme("edit-paste"), tr("Colar"), this);
    pasteAct->setShortcut(QKeySequence::Paste);
    pasteAct->setToolTip(tr("Colar (Ctrl+V)"));
    connect(pasteAct, &QAction::triggered, [this](){ if(currentEditor()) currentEditor()->paste(); });
    mainToolBar->addAction(pasteAct);

    mainToolBar->addSeparator();

    // Word Wrap Toggle
    wordWrapAction = new QAction(QIcon::fromTheme("text-wrap"), tr("Quebra de Linha"), this);
    wordWrapAction->setCheckable(true);
    wordWrapAction->setToolTip(tr("Alternar Quebra de Linha"));
    connect(wordWrapAction, &QAction::toggled, this, &MainWindow::toggleWordWrap);
    mainToolBar->addAction(wordWrapAction);

    mainToolBar->addSeparator();

    // Sair
    QAction *exitAct = new QAction(QIcon::fromTheme("application-exit"), tr("Sair"), this);
    exitAct->setShortcut(QKeySequence::Quit);
    exitAct->setToolTip(tr("Sair (Ctrl+Q)"));
    connect(exitAct, &QAction::triggered, this, &MainWindow::close);
    mainToolBar->addAction(exitAct);
}

void MainWindow::newFile()
{
    addEditorTab();
}

void MainWindow::addEditorTab(const QString &filePath, const QString &content, bool isModified)
{
    if (!filePath.isEmpty()) {
        for (int i = 0; i < tabWidget->count(); ++i) {
            CodeEditor *editor = qobject_cast<CodeEditor*>(tabWidget->widget(i));
            if (editor && editor->property("filePath").toString() == filePath) {
                tabWidget->setCurrentIndex(i);
                return;
            }
        }
    }

    CodeEditor *editor = new CodeEditor();
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            editor->setText(file.readAll());
            file.close();
        }
        editor->setProperty("filePath", filePath);
    }
    
    if (!content.isEmpty()) {
        editor->setText(content);
    }

    editor->setModified(isModified);
    
    QString title = filePath.isEmpty() ? "untitled" : QFileInfo(filePath).fileName();
    if (isModified) title += "*";

    int index = tabWidget->addTab(editor, title);
    tabWidget->setTabToolTip(index, filePath);
    tabWidget->setCurrentIndex(index);

    connect(editor, &CodeEditor::modificationChanged, this, &MainWindow::updateTabTitle);
}

void MainWindow::openFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("Abrir Arquivo"));
    if (!filePath.isEmpty()) {
        addEditorTab(filePath);
    }
}

bool MainWindow::saveFile()
{
    CodeEditor *editor = currentEditor();
    if (!editor) return false;

    QString filePath = editor->property("filePath").toString();
    if (filePath.isEmpty()) {
        return saveFileAs();
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Erro"), tr("Não foi possível salvar o arquivo %1").arg(filePath));
        return false;
    }

    QTextStream out(&file);
    out << editor->text();
    file.close();

    editor->setModified(false);
    updateTabTitle();
    return true;
}

bool MainWindow::saveFileAs()
{
    CodeEditor *editor = currentEditor();
    if (!editor) return false;

    QString filePath = QFileDialog::getSaveFileName(this, tr("Salvar Arquivo Como"));
    if (filePath.isEmpty()) return false;

    editor->setProperty("filePath", filePath);
    return saveFile();
}

void MainWindow::closeTab(int index)
{
    CodeEditor *editor = qobject_cast<CodeEditor*>(tabWidget->widget(index));
    if (editor) {
        if (editor->isModified()) {
            auto res = QMessageBox::question(this, tr("Alterações não salvas"),
                                             tr("A aba possui alterações não salvas. Salvar antes de fechar?"),
                                             QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            if (res == QMessageBox::Save) {
                if (!saveFile()) return;
            } else if (res == QMessageBox::Cancel) {
                return;
            }
        }
        
        // Save to closed tabs stack
        TabState state;
        state.filePath = editor->property("filePath").toString();
        state.isModified = editor->isModified();
        state.unsavedContent = editor->text();
        closedTabsStack.push(state);
    }

    tabWidget->removeTab(index);
    if (tabWidget->count() == 0) {
        newFile();
    }
}

void MainWindow::reopenLastTab()
{
    if (!closedTabsStack.isEmpty()) {
        TabState state = closedTabsStack.pop();
        addEditorTab(state.filePath, state.unsavedContent, state.isModified);
    }
}

void MainWindow::updateTabTitle()
{
    int index = tabWidget->currentIndex();
    CodeEditor *editor = currentEditor();
    if (index != -1 && editor) {
        QString filePath = editor->property("filePath").toString();
        QString title = filePath.isEmpty() ? "untitled" : QFileInfo(filePath).fileName();
        if (editor->isModified()) {
            title += "*";
        }
        tabWidget->setTabText(index, title);
    }
}

void MainWindow::onTabChanged(int index)
{
    if (index != -1) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(tabWidget->widget(index));
        if (editor) {
            QString filePath = editor->property("filePath").toString();
            setWindowTitle(QString("%1 - Simple C++ Editor").arg(filePath.isEmpty() ? "untitled" : filePath));
            
            // Sync Word Wrap toggle state
            wordWrapAction->blockSignals(true);
            wordWrapAction->setChecked(editor->wrapMode() != QsciScintilla::WrapNone);
            wordWrapAction->blockSignals(false);
        }
    }
}

void MainWindow::toggleWordWrap(bool checked)
{
    CodeEditor *editor = currentEditor();
    if (editor) {
        editor->setWrapMode(checked ? QsciScintilla::WrapWord : QsciScintilla::WrapNone);
    }
}

CodeEditor* MainWindow::currentEditor() const
{
    return qobject_cast<CodeEditor*>(tabWidget->currentWidget());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSession();
    event->accept();
}

void MainWindow::saveSession()
{
    QList<TabState> states;
    for (int i = 0; i < tabWidget->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(tabWidget->widget(i));
        if (editor) {
            TabState state;
            state.filePath = editor->property("filePath").toString();
            state.isModified = editor->isModified();
            if (state.isModified) {
                state.unsavedContent = editor->text();
            }
            states.append(state);
        }
    }
    SessionManager::saveSession(states, tabWidget->currentIndex());
}

void MainWindow::loadSession()
{
    QList<TabState> states;
    int currentIndex = 0;
    if (SessionManager::loadSession(states, currentIndex)) {
        for (const auto& state : states) {
            addEditorTab(state.filePath, state.unsavedContent, state.isModified);
        }
        if (currentIndex >= 0 && currentIndex < tabWidget->count()) {
            tabWidget->setCurrentIndex(currentIndex);
        }
    }
}

void MainWindow::createMenus()
{
    // File Menu
    QMenu *fileMenu = menuBar()->addMenu(tr("&Arquivo"));
    
    fileMenu->addAction(QIcon::fromTheme("document-new"), tr("&Novo"), QKeySequence::New, this, &MainWindow::newFile);
    fileMenu->addAction(QIcon::fromTheme("folder-open"), tr("&Abrir"), QKeySequence::Open, this, &MainWindow::openFile);
    fileMenu->addAction(QIcon::fromTheme("media-floppy"), tr("&Salvar"), QKeySequence::Save, this, &MainWindow::saveFile);
    fileMenu->addAction(tr("Salvar &Como..."), QKeySequence::SaveAs, this, &MainWindow::saveFileAs);
    
    // Add reopen last tab to menu
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Reabrir Aba Fechada"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T), this, &MainWindow::reopenLastTab);
    
    fileMenu->addSeparator();
    fileMenu->addAction(QIcon::fromTheme("application-exit"), tr("Sai&r"), QKeySequence::Quit, this, &MainWindow::close);

    // Edit Menu
    QMenu *editMenu = menuBar()->addMenu(tr("&Editar"));
    
    editMenu->addAction(QIcon::fromTheme("edit-undo"), tr("&Desfazer"), QKeySequence::Undo, [this](){ if(currentEditor()) currentEditor()->undo(); });
    editMenu->addAction(QIcon::fromTheme("edit-redo"), tr("&Refazer"), QKeySequence::Redo, [this](){ if(currentEditor()) currentEditor()->redo(); });
    editMenu->addSeparator();
    editMenu->addAction(QIcon::fromTheme("edit-cut"), tr("Recor&tar"), QKeySequence::Cut, [this](){ if(currentEditor()) currentEditor()->cut(); });
    editMenu->addAction(QIcon::fromTheme("edit-copy"), tr("&Copiar"), QKeySequence::Copy, [this](){ if(currentEditor()) currentEditor()->copy(); });
    editMenu->addAction(QIcon::fromTheme("edit-paste"), tr("Co&lar"), QKeySequence::Paste, [this](){ if(currentEditor()) currentEditor()->paste(); });
    editMenu->addSeparator();
    editMenu->addAction(tr("Selecionar &Tudo"), QKeySequence::SelectAll, [this](){ if(currentEditor()) currentEditor()->selectAll(); });
    editMenu->addSeparator();
    
    editMenu->addAction(tr("&Duplicar Linha"), QKeySequence(Qt::CTRL | Qt::Key_D), this, &MainWindow::duplicateLine);
    editMenu->addAction(tr("&Apagar Linha"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L), this, &MainWindow::deleteLine);
    editMenu->addSeparator();
    editMenu->addAction(tr("Mover Linha para &Cima"), QKeySequence(Qt::ALT | Qt::Key_Up), this, &MainWindow::moveLineUp);
    editMenu->addAction(tr("Mover Linha para &Baixo"), QKeySequence(Qt::ALT | Qt::Key_Down), this, &MainWindow::moveLineDown);
    editMenu->addSeparator();
    editMenu->addAction(tr("&Comentar/Descomentar"), QKeySequence(Qt::CTRL | Qt::Key_Q), this, &MainWindow::toggleComment);
    editMenu->addSeparator();
    
    // Shortcuts for Indent/Dedent
    editMenu->addAction(tr("Aumentar Recuo"), QKeySequence(Qt::Key_Tab), this, &MainWindow::indentSelection);
    editMenu->addAction(tr("Diminuir Recuo"), QKeySequence(Qt::SHIFT | Qt::Key_Tab), this, &MainWindow::unindentSelection);

    // Search Menu
    QMenu *searchMenu = menuBar()->addMenu(tr("&Pesquisa"));
    searchMenu->addAction(tr("&Localizar..."), QKeySequence::Find, this, &MainWindow::showFindDialog);
    searchMenu->addAction(tr("&Substituir..."), QKeySequence(Qt::CTRL | Qt::Key_H), this, &MainWindow::showReplaceDialog);
    searchMenu->addAction(tr("Localizar &Próximo"), QKeySequence(Qt::Key_F3), this, &MainWindow::doFindNext);
    searchMenu->addAction(tr("Localizar &Anterior"), QKeySequence(Qt::SHIFT | Qt::Key_F3), this, &MainWindow::doFindPrevious);
}

// Search and Replace Implementation
void MainWindow::showFindDialog()
{
    if (searchDialog) {
        searchDialog->close();
        delete searchDialog;
    }
    searchDialog = new SearchDialog(this, false);
    connect(searchDialog, &SearchDialog::findNext, this, &MainWindow::doFindNext);
    connect(searchDialog, &SearchDialog::findPrevious, this, &MainWindow::doFindPrevious);
    
    searchDialog->show();
    searchDialog->raise();
    searchDialog->activateWindow();
}

void MainWindow::showReplaceDialog()
{
    if (searchDialog) {
        searchDialog->close();
        delete searchDialog;
    }
    searchDialog = new SearchDialog(this, true);
    connect(searchDialog, &SearchDialog::findNext, this, &MainWindow::doFindNext);
    connect(searchDialog, &SearchDialog::findPrevious, this, &MainWindow::doFindPrevious);
    connect(searchDialog, &SearchDialog::replace, this, &MainWindow::doReplace);
    connect(searchDialog, &SearchDialog::replaceAll, this, &MainWindow::doReplaceAll);
    
    searchDialog->show();
    searchDialog->raise();
    searchDialog->activateWindow();
}

void MainWindow::doFindNext()
{
    CodeEditor *editor = currentEditor();
    if (editor && searchDialog) {
        bool found = editor->findNext();
        if (!found) {
            // Try from beginning (wrap)
            editor->findFirst(searchDialog->getSearchText(), 
                              searchDialog->isRegex(), 
                              searchDialog->isCaseSensitive(), 
                              searchDialog->isWholeWord(), 
                              true); // wrap
        }
    }
}

void MainWindow::doFindPrevious()
{
    CodeEditor *editor = currentEditor();
    if (editor && searchDialog) {
        editor->findFirst(searchDialog->getSearchText(), 
                          searchDialog->isRegex(), 
                          searchDialog->isCaseSensitive(), 
                          searchDialog->isWholeWord(), 
                          true, // wrap
                          false); // forward = false
    }
}

void MainWindow::doReplace()
{
    CodeEditor *editor = currentEditor();
    if (editor && searchDialog) {
        editor->replace(searchDialog->getReplaceText());
        doFindNext();
    }
}

void MainWindow::doReplaceAll()
{
    CodeEditor *editor = currentEditor();
    if (editor && searchDialog) {
        while (editor->findNext()) {
            editor->replace(searchDialog->getReplaceText());
        }
    }
}

// Text manipulation Slots
void MainWindow::duplicateLine() { if (currentEditor()) currentEditor()->duplicateLine(); }
void MainWindow::deleteLine() { if (currentEditor()) currentEditor()->deleteLine(); }
void MainWindow::moveLineUp() { if (currentEditor()) currentEditor()->moveLineUp(); }
void MainWindow::moveLineDown() { if (currentEditor()) currentEditor()->moveLineDown(); }
void MainWindow::toggleComment() { if (currentEditor()) currentEditor()->toggleComment(); }
void MainWindow::indentSelection() { if (currentEditor()) currentEditor()->indentSelection(); }
void MainWindow::unindentSelection() { if (currentEditor()) currentEditor()->unindentSelection(); }
