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

    menuBar()->hide();
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
    QToolBar *mainToolBar = addToolBar(tr("Main"));
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

    // Save As
    QAction *saveAsAct = new QAction(QIcon::fromTheme("document-save-as"), tr("Salvar Como"), this);
    if (saveAsAct->icon().isNull()) {
        saveAsAct->setIcon(QIcon::fromTheme("media-floppy"));
    }
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    saveAsAct->setToolTip(tr("Salvar Como..."));
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveFileAs);
    mainToolBar->addAction(saveAsAct);

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

    // Reopen closed tab shortcut
    QAction *reopenAct = new QAction(this);
    reopenAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    reopenAct->setShortcutContext(Qt::WindowShortcut);
    connect(reopenAct, &QAction::triggered, this, &MainWindow::reopenLastTab);
    addAction(reopenAct);
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
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"));
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
        QMessageBox::warning(this, tr("Error"), tr("Cannot save file %1").arg(filePath));
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

    QString filePath = QFileDialog::getSaveFileName(this, tr("Save File As"));
    if (filePath.isEmpty()) return false;

    editor->setProperty("filePath", filePath);
    return saveFile();
}

void MainWindow::closeTab(int index)
{
    CodeEditor *editor = qobject_cast<CodeEditor*>(tabWidget->widget(index));
    if (editor) {
        if (editor->isModified()) {
            auto res = QMessageBox::question(this, tr("Unsaved Changes"),
                                             tr("Tab has unsaved changes. Save before closing?"),
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
