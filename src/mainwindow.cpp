#include "mainwindow.h"
#include "sessionmanager.h"
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QCloseEvent>
#include <QPushButton>
#include <QToolBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    tabWidget = new QTabWidget(this);
    tabWidget->setTabsClosable(true);
    tabWidget->setMovable(true);
    tabWidget->setAttribute(Qt::WA_AcceptDrops, false);

    // Create Notification Bar
    notificationBar = new QWidget(this);
    notificationBar->setObjectName("notificationBar");
    // Yellow warning background with a slightly darker bottom border
    notificationBar->setStyleSheet("QWidget#notificationBar { background-color: #fff3cd; border-bottom: 1px solid #ffeeba; }");
    notificationBar->setVisible(false);

    notificationLabel = new QLabel(this);
    // Darker text for contrast on yellow background
    notificationLabel->setStyleSheet("color: #856404; font-size: 13px;");
    
    reloadButton = new QPushButton(tr("Recarregar do Disco"), this);
    // Style button to look like a clean, clickable action without default gray background
    reloadButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #856404;"
        "  color: white;"
        "  border-radius: 3px;"
        "  padding: 4px 12px;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "  border: none;"
        "}"
        "QPushButton:hover {"
        "  background-color: #6d5203;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #564102;"
        "}"
    );
    connect(reloadButton, &QPushButton::clicked, this, &MainWindow::reloadCurrentFile);

    QHBoxLayout *notifyLayout = new QHBoxLayout(notificationBar);
    notifyLayout->setContentsMargins(10, 2, 10, 2);
    notifyLayout->addWidget(notificationLabel);
    notifyLayout->addStretch();
    notifyLayout->addWidget(reloadButton);

    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(notificationBar);
    mainLayout->addWidget(tabWidget);

    setCentralWidget(centralWidget);

    // Setup File Watcher
    fileWatcher = new QFileSystemWatcher(this);
    connect(fileWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::handleFileChanged);

    connect(tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    createActions();
    createMenus();
    createToolBar();
    
    resize(800, 600);
    loadSession();

    if (tabWidget->count() == 0) {
        newFile();
    }

    setWindowTitle("Simple C++ Editor");
    setWindowIcon(QIcon::fromTheme("text-x-generic"));
    setAcceptDrops(true);
    
    // Forçamos a janela a nunca abrir maximizada
    showNormal();

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

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    
    // No Wayland, o estado da janela (maximized) pode demorar alguns ms para atualizar.
    // Usamos um timer curto para validar se o tamanho novo é realmente um tamanho "normal".
    QTimer::singleShot(100, this, [this]() {
        if (!(windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen | Qt::WindowMinimized))) {
            normalWidth = width();
            normalHeight = height();
        }
    });
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
    connect(saveAct, &QAction::triggered, [this](){ saveFile(true); });

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

    changeThemeAct = new QAction(tr("&Temas..."), this);
    connect(changeThemeAct, &QAction::triggered, this, &MainWindow::showThemeDialog);

    wordWrapAction = new QAction(QIcon::fromTheme("text-wrap"), tr("Quebra de Linha"), this);
    wordWrapAction->setCheckable(true);
    wordWrapAction->setToolTip(tr("Alternar Quebra de Linha"));
    connect(wordWrapAction, &QAction::toggled, this, &MainWindow::toggleWordWrap);
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

    // Word Wrap Toggle
    mainToolBar->addAction(wordWrapAction);

    mainToolBar->addSeparator();

    mainToolBar->addAction(exitAct);
}

void MainWindow::newFile()
{
    addEditorTab();
}

void MainWindow::addEditorTab(const QString &filePath, const QString &content, bool isModified, const QString &language)
{
    QString absPath;
    if (!filePath.isEmpty()) {
        absPath = QFileInfo(filePath).absoluteFilePath();
        for (int i = 0; i < tabWidget->count(); ++i) {
            CodeEditor *editor = qobject_cast<CodeEditor*>(tabWidget->widget(i));
            if (editor && editor->property("filePath").toString() == absPath) {
                tabWidget->setCurrentIndex(i);
                return;
            }
        }
    }

    CodeEditor *editor = new CodeEditor();
    
    // Apply current theme
    editor->setTheme(ThemeDialog::getAvailableThemes().at(currentThemeIndex));

    if (!absPath.isEmpty()) {
        editor->setProperty("filePath", absPath);
        // Only read from file if we don't already have unsaved content from session
        if (content.isEmpty()) {
            QFile file(absPath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                editor->setText(file.readAll());
                file.close();
            }
        }

        // Start watching the file if it exists
        if (QFile::exists(absPath)) {
            fileWatcher->addPath(absPath);
        }
    }

    if (!content.isEmpty()) {
        editor->setText(content);
    }

    editor->setModified(isModified);

    if (!absPath.isEmpty()) {
        editor->detectLexer(absPath);
    }
    
    if (!language.isEmpty()) {
        editor->setLanguage(language);
    }

    // Apply current theme again to ensure lexer gets the colors
    editor->setTheme(ThemeDialog::getAvailableThemes().at(currentThemeIndex));

    QString title = absPath.isEmpty() ? "untitled" : QFileInfo(absPath).fileName();
    int index = tabWidget->addTab(editor, title);

    // Explicitly update title to ensure asterisk shows if it came from session as modified
    updateTabTitle(index);

    tabWidget->setTabToolTip(index, absPath);
    tabWidget->setCurrentIndex(index);


    connect(editor, &CodeEditor::modificationChanged, [this, editor]() {
        // Find the index of this editor in case it moved
        int idx = tabWidget->indexOf(editor);
        if (idx != -1) {
            updateTabTitle(idx);
        }
    });
}

void MainWindow::openFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("Abrir Arquivo"));
    if (!filePath.isEmpty()) {
        addEditorTab(filePath);
    }
}

bool MainWindow::saveFile(bool forcePathCheck)
{
    CodeEditor *editor = currentEditor();
    if (!editor) return false;

    QString filePath = editor->property("filePath").toString();
    
    // LOGIC:
    // 1. If no path, we MUST Save As.
    // 2. If path exists but file is MISSING from disk AND we are not coming from saveFileAs, we MUST Save As.
    if (filePath.isEmpty() || (forcePathCheck && !QFile::exists(filePath))) {
        return saveFileAs();
    }

    // Explicitly remove from watcher to avoid race condition during save
    fileWatcher->removePath(filePath);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Erro"), tr("Não foi possível salvar o arquivo %1").arg(filePath));
        fileWatcher->addPath(filePath);
        return false;
    }

    QTextStream out(&file);
    out << editor->text();
    file.flush();
    file.close();

    // Re-add to watcher after close
    fileWatcher->addPath(filePath);

    // Clear external modification flags for this file
    externallyModifiedFiles.remove(filePath);
    hideNotification();

    editor->setModified(false);
    updateTabTitle();
    return true;
}

bool MainWindow::saveFileAs()
{
    CodeEditor *editor = currentEditor();
    if (!editor) return false;

    QString oldPath = editor->property("filePath").toString();
    QString filePath = QFileDialog::getSaveFileName(this, tr("Salvar Arquivo Como"), oldPath);
    if (filePath.isEmpty()) return false;

    if (!oldPath.isEmpty()) {
        fileWatcher->removePath(oldPath);
    }

    editor->setProperty("filePath", filePath);
    editor->detectLexer(filePath);
    editor->setTheme(ThemeDialog::getAvailableThemes().at(currentThemeIndex));
    
    return saveFile(false); // Pass false to skip existence check for the new path
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
        QString filePath = editor->property("filePath").toString();
        if (!filePath.isEmpty()) {
            fileWatcher->removePath(filePath);
        }
        state.filePath = filePath;
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

void MainWindow::updateTabTitle(int index)
{
    if (index == -1) {
        index = tabWidget->currentIndex();
    }
    
    if (index == -1) return;

    CodeEditor *editor = qobject_cast<CodeEditor*>(tabWidget->widget(index));
    if (editor) {
        QString filePath = editor->property("filePath").toString();
        QString title = filePath.isEmpty() ? "untitled" : QFileInfo(filePath).fileName();
        bool isExternallyModified = !filePath.isEmpty() && externallyModifiedFiles.value(filePath, false);
        if (editor->isModified() || isExternallyModified) {
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
            
            // Check for external modifications
            if (!filePath.isEmpty() && externallyModifiedFiles.value(filePath, false)) {
                QString absPath = QFileInfo(filePath).absoluteFilePath();
                bool exists = QFile::exists(absPath);
                QString fileName = QFileInfo(absPath).fileName();
                
                if (!exists) {
                    notificationLabel->setText(tr("O arquivo \"%1\" foi removido do disco.").arg(fileName));
                    reloadButton->setVisible(false);
                } else {
                    notificationLabel->setText(tr("O arquivo \"%1\" foi alterado externamente.").arg(fileName));
                    reloadButton->setVisible(true);
                }
                notificationBar->setVisible(true);
            } else {
                notificationBar->setVisible(false);
            }

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
            QString filePath = editor->property("filePath").toString();
            state.filePath = filePath;
            state.language = editor->currentLanguage();
            
            bool isExternallyModified = !filePath.isEmpty() && externallyModifiedFiles.value(filePath, false);
            state.isModified = editor->isModified() || isExternallyModified;
            
            // Save content if it's modified OR if the file was deleted/changed externally
            // so we can restore it exactly as it was.
            if (state.isModified) {
                state.unsavedContent = editor->text();
            }
            states.append(state);
        }
    }
    SessionManager::saveSession(states, tabWidget->currentIndex(), normalWidth, normalHeight);
}

void MainWindow::loadSession()
{
    QList<TabState> states;
    int currentIndex = 0;
    int w = 800;
    int h = 600;
    
    if (SessionManager::loadSession(states, currentIndex, w, h)) {
        for (const auto& state : states) {
            addEditorTab(state.filePath, state.unsavedContent, state.isModified, state.language);
        }
        if (currentIndex >= 0 && currentIndex < tabWidget->count()) {
            tabWidget->setCurrentIndex(currentIndex);
        }
        
        // Restore size with minimum constraints
        this->normalWidth = qMax(300, w);
        this->normalHeight = qMax(300, h);
        resize(this->normalWidth, this->normalHeight);
    }
}

void MainWindow::setLanguage(const QString &lang)
{
    CodeEditor *editor = currentEditor();
    if (editor) {
        editor->setLanguage(lang);
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

    // View/Themes Menu
    QMenu *viewMenu = menuBar()->addMenu(tr("&Exibir"));
    viewMenu->addAction(wordWrapAction);
    viewMenu->addSeparator();
    viewMenu->addAction(changeThemeAct);

    // Language Menu
    QMenu *langMenu = menuBar()->addMenu(tr("&Linguagem"));
    QStringList languages = {"Plain Text", "C++", "HTML", "CSS", "JavaScript", "JSON", "Python", "XML", "Bash", "SQL"};
    for (const QString &lang : languages) {
        langMenu->addAction(lang, [this, lang]() {
            setLanguage(lang == "Plain Text" ? "" : lang);
        });
    }
}

void MainWindow::showThemeDialog()
{
    ThemeDialog *dialog = new ThemeDialog(this);
    dialog->setSelectedThemeIndex(currentThemeIndex);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    
    connect(dialog, &ThemeDialog::themeSelected, this, &MainWindow::applyTheme);
    
    dialog->show();
}

void MainWindow::applyTheme(int index)
{
    currentThemeIndex = index;
    EditorTheme theme = ThemeDialog::getAvailableThemes().at(currentThemeIndex);
    
    // Apply to all open editors
    for (int i = 0; i < tabWidget->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(tabWidget->widget(i));
        if (editor) {
            editor->setTheme(theme);
        }
    }
}

void MainWindow::handleFileChanged(const QString &path)
{
    QString absChangedPath = QFileInfo(path).absoluteFilePath();
    
    // Find the tab for this path
    CodeEditor *targetEditor = nullptr;
    int tabIndex = -1;
    for (int i = 0; i < tabWidget->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(tabWidget->widget(i));
        if (editor) {
            QString editorPath = editor->property("filePath").toString();
            if (!editorPath.isEmpty() && QFileInfo(editorPath).absoluteFilePath() == absChangedPath) {
                targetEditor = editor;
                tabIndex = i;
                break;
            }
        }
    }

    if (!targetEditor) return;

    bool exists = QFile::exists(absChangedPath);
    // Mark as externally modified to ensure notification shows up in onTabChanged too
    externallyModifiedFiles[absChangedPath] = true;

    if (!exists) {
        // File deleted externally
        targetEditor->setModified(true);
        updateTabTitle(tabIndex);
    } else {
        // File modified externally
        updateTabTitle(tabIndex);
    }

    // Show notification if it's the current tab
    if (tabWidget->currentIndex() == tabIndex) {
        QString fileName = QFileInfo(absChangedPath).fileName();
        if (!exists) {
            notificationLabel->setText(tr("O arquivo \"%1\" foi removido do disco.").arg(fileName));
            reloadButton->setVisible(false);
        } else {
            notificationLabel->setText(tr("O arquivo \"%1\" foi alterado externamente.").arg(fileName));
            reloadButton->setVisible(true);
        }
        notificationBar->setVisible(true);
    }

    // Re-add to watcher if needed (some systems remove it on change/delete)
    if (!fileWatcher->files().contains(absChangedPath) && exists) {
        fileWatcher->addPath(absChangedPath);
    }
}

void MainWindow::reloadCurrentFile()
{
    CodeEditor *editor = currentEditor();
    if (!editor) return;

    QString path = editor->property("filePath").toString();
    if (path.isEmpty()) return;

    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        editor->setText(file.readAll());
        file.close();
        editor->setModified(false);
        externallyModifiedFiles.remove(path);
        hideNotification();
        updateTabTitle();
    } else {
        QMessageBox::warning(this, tr("Erro"), tr("Não foi possível recarregar o arquivo \"%1\".").arg(path));
    }
}

void MainWindow::hideNotification()
{
    notificationBar->setVisible(false);
}

// Search and Replace Implementation
void MainWindow::showFindDialog()
{
    CodeEditor *editor = currentEditor();
    if (!editor) return;

    if (searchDialog) {
        searchDialog->close();
        delete searchDialog;
    }
    searchDialog = new SearchDialog(this, false);
    
    if (editor->hasSelectedText()) {
        searchDialog->setSearchText(editor->selectedText());
    }

    connect(searchDialog, &SearchDialog::findNext, this, &MainWindow::doFindNext);
    connect(searchDialog, &SearchDialog::findPrevious, this, &MainWindow::doFindPrevious);
    
    searchDialog->show();
    searchDialog->raise();
    searchDialog->activateWindow();
}

void MainWindow::showReplaceDialog()
{
    CodeEditor *editor = currentEditor();
    if (!editor) return;

    if (searchDialog) {
        searchDialog->close();
        delete searchDialog;
    }
    searchDialog = new SearchDialog(this, true);

    if (editor->hasSelectedText()) {
        searchDialog->setSearchText(editor->selectedText());
    }

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
        QString term = searchDialog->getSearchText();
        if (term.isEmpty()) return;

        int line, index;
        if (editor->hasSelectedText()) {
            int lineFrom, indexFrom, lineTo, indexTo;
            editor->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
            line = lineTo;
            index = indexTo;
        } else {
            editor->getCursorPosition(&line, &index);
        }

        bool found = editor->findFirst(term,
                                       searchDialog->isRegex(),
                                       searchDialog->isCaseSensitive(),
                                       searchDialog->isWholeWord(),
                                       true,  // wrap
                                       true,  // forward
                                       line,
                                       index,
                                       true); // show
        
        if (!found) {
            // Not found message?
        }
    }
}

void MainWindow::doFindPrevious()
{
    CodeEditor *editor = currentEditor();
    if (editor && searchDialog) {
        QString term = searchDialog->getSearchText();
        if (term.isEmpty()) return;

        int line, index;
        if (editor->hasSelectedText()) {
            int lineFrom, indexFrom, lineTo, indexTo;
            editor->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
            line = lineFrom;
            index = indexFrom;
        } else {
            editor->getCursorPosition(&line, &index);
        }

        editor->findFirst(term,
                          searchDialog->isRegex(),
                          searchDialog->isCaseSensitive(),
                          searchDialog->isWholeWord(),
                          true,  // wrap
                          false, // forward = false
                          line,
                          index,
                          true); // show
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
