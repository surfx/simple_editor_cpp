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

    createActions();
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

void MainWindow::createActions()
{
    // File Actions
    newAct = new QAction(QIcon::fromTheme("document-new"), tr("&Novo"), this);
    newAct->setShortcut(QKeySequence::New);
    newAct->setShortcutContext(Qt::WindowShortcut);
    connect(newAct, &QAction::triggered, this, &MainWindow::newFile);

    openAct = new QAction(QIcon::fromTheme("folder-open"), tr("&Abrir"), this);
    openAct->setShortcut(QKeySequence::Open);
    openAct->setShortcutContext(Qt::WindowShortcut);
    connect(openAct, &QAction::triggered, this, &MainWindow::openFile);

    saveAct = new QAction(QIcon::fromTheme("media-floppy"), tr("&Salvar"), this);
    saveAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    saveAct->setShortcutContext(Qt::WindowShortcut);
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveFile);

    saveAsAct = new QAction(tr("Salvar &Como..."), this);
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    saveAsAct->setShortcutContext(Qt::WindowShortcut);
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveFileAs);

    exitAct = new QAction(QIcon::fromTheme("application-exit"), tr("Sai&r"), this);
    exitAct->setShortcut(QKeySequence::Quit);
    exitAct->setShortcutContext(Qt::WindowShortcut);
    connect(exitAct, &QAction::triggered, this, &MainWindow::close);

    // Edit Actions
    undoAct = new QAction(QIcon::fromTheme("edit-undo"), tr("&Desfazer"), this);
    undoAct->setShortcut(QKeySequence::Undo);
    undoAct->setShortcutContext(Qt::WindowShortcut);
    connect(undoAct, &QAction::triggered, [this](){ if(currentEditor()) currentEditor()->undo(); });

    redoAct = new QAction(QIcon::fromTheme("edit-redo"), tr("&Refazer"), this);
    redoAct->setShortcut(QKeySequence::Redo);
    redoAct->setShortcutContext(Qt::WindowShortcut);
    connect(redoAct, &QAction::triggered, [this](){ if(currentEditor()) currentEditor()->redo(); });

    cutAct = new QAction(QIcon::fromTheme("edit-cut"), tr("Recor&tar"), this);
    cutAct->setShortcut(QKeySequence::Cut);
    cutAct->setShortcutContext(Qt::WindowShortcut);
    connect(cutAct, &QAction::triggered, [this](){ if(currentEditor()) currentEditor()->cut(); });

    copyAct = new QAction(QIcon::fromTheme("edit-copy"), tr("&Copiar"), this);
    copyAct->setShortcut(QKeySequence::Copy);
    copyAct->setShortcutContext(Qt::WindowShortcut);
    connect(copyAct, &QAction::triggered, [this](){ if(currentEditor()) currentEditor()->copy(); });

    pasteAct = new QAction(QIcon::fromTheme("edit-paste"), tr("Co&lar"), this);
    pasteAct->setShortcut(QKeySequence::Paste);
    pasteAct->setShortcutContext(Qt::WindowShortcut);
    connect(pasteAct, &QAction::triggered, [this](){ if(currentEditor()) currentEditor()->paste(); });

    selectAllAct = new QAction(tr("Selecionar &Tudo"), this);
    selectAllAct->setShortcut(QKeySequence::SelectAll);
    selectAllAct->setShortcutContext(Qt::WindowShortcut);
    connect(selectAllAct, &QAction::triggered, [this](){ if(currentEditor()) currentEditor()->selectAll(); });

    duplicateLineAct = new QAction(tr("&Duplicar Linha"), this);
    duplicateLineAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    duplicateLineAct->setShortcutContext(Qt::WindowShortcut);
    connect(duplicateLineAct, &QAction::triggered, this, &MainWindow::duplicateLine);

    deleteLineAct = new QAction(tr("&Apagar Linha"), this);
    deleteLineAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
    deleteLineAct->setShortcutContext(Qt::WindowShortcut);
    connect(deleteLineAct, &QAction::triggered, this, &MainWindow::deleteLine);

    moveLineUpAct = new QAction(tr("Mover Linha para &Cima"), this);
    moveLineUpAct->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
    moveLineUpAct->setShortcutContext(Qt::WindowShortcut);
    connect(moveLineUpAct, &QAction::triggered, this, &MainWindow::moveLineUp);

    moveLineDownAct = new QAction(tr("Mover Linha para &Baixo"), this);
    moveLineDownAct->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Down));
    moveLineDownAct->setShortcutContext(Qt::WindowShortcut);
    connect(moveLineDownAct, &QAction::triggered, this, &MainWindow::moveLineDown);

    toggleCommentAct = new QAction(tr("&Comentar/Descomentar"), this);
    toggleCommentAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    toggleCommentAct->setShortcutContext(Qt::WindowShortcut);
    connect(toggleCommentAct, &QAction::triggered, this, &MainWindow::toggleComment);

    indentAct = new QAction(tr("Aumentar Recuo"), this);
    indentAct->setShortcut(QKeySequence(Qt::Key_Tab));
    indentAct->setShortcutContext(Qt::WindowShortcut);
    connect(indentAct, &QAction::triggered, this, &MainWindow::indentSelection);

    unindentAct = new QAction(tr("Diminuir Recuo"), this);
    unindentAct->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Tab));
    unindentAct->setShortcutContext(Qt::WindowShortcut);
    connect(unindentAct, &QAction::triggered, this, &MainWindow::unindentSelection);

    // Search Actions
    findAct = new QAction(tr("&Localizar..."), this);
    findAct->setShortcut(QKeySequence::Find);
    findAct->setShortcutContext(Qt::WindowShortcut);
    connect(findAct, &QAction::triggered, this, &MainWindow::showFindDialog);

    replaceAct = new QAction(tr("&Substituir..."), this);
    replaceAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
    replaceAct->setShortcutContext(Qt::WindowShortcut);
    connect(replaceAct, &QAction::triggered, this, &MainWindow::showReplaceDialog);

    findNextAct = new QAction(tr("Localizar &Próximo"), this);
    findNextAct->setShortcut(QKeySequence(Qt::Key_F3));
    findNextAct->setShortcutContext(Qt::WindowShortcut);
    connect(findNextAct, &QAction::triggered, this, &MainWindow::doFindNext);

    findPrevAct = new QAction(tr("Localizar &Anterior"), this);
    findPrevAct->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
    findPrevAct->setShortcutContext(Qt::WindowShortcut);
    connect(findPrevAct, &QAction::triggered, this, &MainWindow::doFindPrevious);

    reopenAct = new QAction(tr("Reabrir Aba Fechada"), this);
    reopenAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    reopenAct->setShortcutContext(Qt::WindowShortcut);
    connect(reopenAct, &QAction::triggered, this, &MainWindow::reopenLastTab);
}

void MainWindow::createToolBar()
{
    QToolBar *mainToolBar = addToolBar(tr("Principal"));
    mainToolBar->setMovable(false);
    mainToolBar->setFloatable(false);
    mainToolBar->setIconSize(QSize(24, 24));

    mainToolBar->addAction(newAct);
    mainToolBar->addAction(openAct);
    mainToolBar->addAction(saveAct);

    mainToolBar->addSeparator();

    mainToolBar->addAction(undoAct);
    mainToolBar->addAction(redoAct);

    mainToolBar->addSeparator();

    mainToolBar->addAction(cutAct);
    mainToolBar->addAction(copyAct);
    mainToolBar->addAction(pasteAct);

    mainToolBar->addSeparator();

    // Word Wrap Toggle (Special case as it's a member)
    wordWrapAction = new QAction(QIcon::fromTheme("text-wrap"), tr("Quebra de Linha"), this);
    wordWrapAction->setCheckable(true);
    wordWrapAction->setToolTip(tr("Alternar Quebra de Linha"));
    connect(wordWrapAction, &QAction::toggled, this, &MainWindow::toggleWordWrap);
    mainToolBar->addAction(wordWrapAction);

    mainToolBar->addSeparator();

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
    fileMenu->addAction(newAct);
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    fileMenu->addAction(saveAsAct);
    
    // Add reopen last tab to menu
    fileMenu->addSeparator();
    fileMenu->addAction(reopenAct);
    
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    // Edit Menu
    QMenu *editMenu = menuBar()->addMenu(tr("&Editar"));
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);
    editMenu->addSeparator();
    editMenu->addAction(cutAct);
    editMenu->addAction(copyAct);
    editMenu->addAction(pasteAct);
    editMenu->addSeparator();
    editMenu->addAction(selectAllAct);
    editMenu->addSeparator();
    
    editMenu->addAction(duplicateLineAct);
    editMenu->addAction(deleteLineAct);
    editMenu->addSeparator();
    editMenu->addAction(moveLineUpAct);
    editMenu->addAction(moveLineDownAct);
    editMenu->addSeparator();
    editMenu->addAction(toggleCommentAct);
    editMenu->addSeparator();
    
    editMenu->addAction(indentAct);
    editMenu->addAction(unindentAct);

    // Search Menu
    QMenu *searchMenu = menuBar()->addMenu(tr("&Pesquisa"));
    searchMenu->addAction(findAct);
    searchMenu->addAction(replaceAct);
    searchMenu->addAction(findNextAct);
    searchMenu->addAction(findPrevAct);
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
