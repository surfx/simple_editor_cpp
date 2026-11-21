#include "mainwindow.h"
#include "sessionmanager.h"
#include "transparencydialog.h"
#include <QGuiApplication>
#include <QMenuBar>
#include <QMenu>
#include <QSet>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QCloseEvent>
#include <QPushButton>
#include <QToolBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QWindow>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace {
bool isKdeWaylandSession()
{
    static const bool result = []() {
        return QGuiApplication::platformName().contains(QLatin1String("wayland"), Qt::CaseInsensitive)
            && qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains(QLatin1String("KDE"), Qt::CaseInsensitive);
    }();
    return result;
}
}

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

    // Status Bar
    statusModifiedLabel = new QLabel(this);
    statusEncodingLabel = new QLabel(tr("UTF-8"), this);
    statusLanguageLabel = new QLabel(this);
    statusPositionLabel = new QLabel(this);
    for (QLabel *label : {statusModifiedLabel, statusLanguageLabel, statusEncodingLabel, statusPositionLabel}) {
        label->setContentsMargins(6, 0, 6, 0);
    }
    statusBar()->addPermanentWidget(statusModifiedLabel);
    statusBar()->addPermanentWidget(statusLanguageLabel);
    statusBar()->addPermanentWidget(statusEncodingLabel);
    statusBar()->addPermanentWidget(statusPositionLabel);

    recentClosedFiles = SessionManager::loadRecentClosedFiles();

    createActions();
    createMenus();
    createToolBar();

    // Restore global word wrap preference
    wordWrapAction->setChecked(SessionManager::loadWordWrapEnabled());

    // Restore window transparency preference. The actual opacity is applied
    // after the window is shown, once the compositor knows the window.
    windowOpacity = SessionManager::loadWindowOpacity();

    transparencyTimer = new QTimer(this);
    transparencyTimer->setSingleShot(true);
    transparencyTimer->setInterval(120);
    connect(transparencyTimer, &QTimer::timeout, this, &MainWindow::applyTransparency);

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

    // Apply the saved transparency only after the window has been mapped.
    QTimer::singleShot(300, this, [this]() {
        applyWindowOpacity(windowOpacity);
    });

    // Setup Single Instance Server
    localServer = new QLocalServer(this);
    // Try to claim the server name first. Only remove a stale/orphaned socket
    // file as a fallback, so we don't blindly kick out a legitimately running
    // instance if two processes happen to start at nearly the same time.
    if (!localServer->listen("SimpleEditorServer")) {
        QLocalServer::removeServer("SimpleEditorServer");
        localServer->listen("SimpleEditorServer");
    }
    if (localServer->isListening()) {
        connect(localServer, &QLocalServer::newConnection, this, &MainWindow::newLocalConnection);
    }
}

MainWindow::~MainWindow()
{
}

void MainWindow::newLocalConnection()
{
    QLocalSocket *socket = localServer->nextPendingConnection();
    if (!socket) {
        return;
    }

    // Cada cliente envia uma ou mais linhas JSON (uma por pedido). Acumulamos
    // os bytes até ter uma linha completa, pois o socket é um fluxo e o
    // readyRead pode entregar pedaços parciais da mensagem.
    QByteArray *buffer = new QByteArray;

    connect(socket, &QLocalSocket::readyRead, this, [this, socket, buffer]() {
        buffer->append(socket->readAll());

        int newline;
        while ((newline = buffer->indexOf('\n')) >= 0) {
            const QByteArray payload = buffer->left(newline);
            buffer->remove(0, newline + 1);
            if (!payload.trimmed().isEmpty()) {
                handleClientMessage(payload);
            }
        }
    });

    connect(socket, &QLocalSocket::disconnected, this, [this, socket, buffer]() {
        // Compatibilidade com versões antigas, que enviavam o caminho sem quebra de linha.
        if (!buffer->trimmed().isEmpty()) {
            handleClientMessage(*buffer);
        }
        delete buffer;
        socket->deleteLater();
    });
}

void MainWindow::handleClientMessage(const QByteArray &payload)
{
    QStringList files;
    QString activationToken;

    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);

    if (error.error == QJsonParseError::NoError && document.isObject()) {
        const QJsonObject request = document.object();
        const QJsonArray fileArray = request.value(QStringLiteral("files")).toArray();
        for (const QJsonValue &value : fileArray) {
            if (value.isString()) {
                files << value.toString();
            }
        }
        activationToken = request.value(QStringLiteral("activationToken")).toString();
    } else {
        // Cliente antigo: o payload é o próprio caminho do arquivo.
        const QString legacyPath = QString::fromUtf8(payload);
        if (!legacyPath.isEmpty()) {
            files << legacyPath;
        }
    }

    for (const QString &filePath : files) {
        addEditorTab(filePath);
    }

    activateFromExternalRequest(activationToken);
}

void MainWindow::activateFromExternalRequest(const QString &activationToken)
{
    // Restaura a janela se estiver minimizada, sem perder o estado de maximizada.
    if (isMinimized()) {
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    }
    show();
    raise();

    // No Wayland quem decide o foco é o compositor. O token xdg-activation
    // recebido da instância secundária (herdado do lançador, ex.: Dolphin) é a
    // forma suportada de pedir o foco para esta janela. O Qt lê a variável
    // abaixo no momento do requestActivate().
    if (!activationToken.isEmpty()) {
        qputenv("XDG_ACTIVATION_TOKEN", activationToken.toUtf8());
        if (QWindow *window = windowHandle()) {
            window->requestActivate();
        }
        qunsetenv("XDG_ACTIVATION_TOKEN");
    }

    activateWindow();

    // Se o compositor ainda não deu o foco (por exemplo, quando não veio token
    // do lançador), pede a ativação diretamente ao KWin. Só roda no Plasma/Wayland.
    QTimer::singleShot(150, this, [this]() {
        if (!isActiveWindow()) {
            activateWindowOnKdeWayland();
        }
    });
}

void MainWindow::activateWindowOnKdeWayland()
{
    static const bool kdeWayland = []() {
        return qEnvironmentVariable("XDG_SESSION_TYPE") == QLatin1String("wayland")
            && qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains(QLatin1String("KDE"), Qt::CaseInsensitive);
    }();

    if (!kdeWayland) {
        return;
    }

    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }

    // O token xdg-activation é o caminho padrão, mas nem todo lançador o envia
    // (ex.: abrir pelo terminal). Nesse caso usamos a interface de scripting do
    // KWin, que também é quem decide qual janela recebe o foco no Wayland.
    static int scriptCounter = 0;
    const QString pluginName = QStringLiteral("simple-editor-activate-%1").arg(++scriptCounter);
    const QString scriptPath = QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation))
                                   .filePath(pluginName + QStringLiteral(".js"));

    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    scriptFile.write(R"JS(
(function () {
    const windows = workspace.windowList ? workspace.windowList() : workspace.clientList();
    for (const window of windows) {
        if (String(window.resourceClass).toLowerCase().indexOf('simpleeditor') === -1)
            continue;
        if (window.minimized)
            window.minimized = false;
        workspace.activeWindow = window;
        break;
    }
})();
)JS");
    scriptFile.close();

    const QString service = QStringLiteral("org.kde.KWin");
    const QString path = QStringLiteral("/Scripting");
    const QString iface = QStringLiteral("org.kde.kwin.Scripting");

    QDBusMessage load = QDBusMessage::createMethodCall(service, path, iface, QStringLiteral("loadScript"));
    load << scriptPath << pluginName;
    const QDBusMessage loadReply = bus.call(load, QDBus::Block, 250);

    if (loadReply.type() == QDBusMessage::ErrorMessage) {
        QFile::remove(scriptPath);
        return;
    }

    // "start" executa os scripts carregados de forma síncrona, então já é
    // seguro descarregar o script logo em seguida.
    bus.call(QDBusMessage::createMethodCall(service, path, iface, QStringLiteral("start")),
             QDBus::Block, 250);

    QDBusMessage unload = QDBusMessage::createMethodCall(service, path, iface, QStringLiteral("unloadScript"));
    unload << pluginName;
    bus.call(unload, QDBus::Block, 250);

    QFile::remove(scriptPath);
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

    // Close Tab Action (Ctrl+W)
    closeTabAct = new QAction(tr("Fechar Aba"), this);
    closeTabAct->setShortcut(QKeySequence::Close);
    closeTabAct->setShortcutContext(Qt::WindowShortcut);
    connect(closeTabAct, &QAction::triggered, this, [this]() {
        int currentIndex = tabWidget->currentIndex();
        if (currentIndex != -1) {
            closeTab(currentIndex);
        }
    });

    changeThemeAct = new QAction(tr("&Temas..."), this);
    connect(changeThemeAct, &QAction::triggered, this, &MainWindow::showThemeDialog);

    transparencyAct = new QAction(tr("&Transparência..."), this);
    connect(transparencyAct, &QAction::triggered, this, &MainWindow::showTransparencyDialog);

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
                editor->setFocus();
                return;
            }
        }
    }

    CodeEditor *editor = new CodeEditor();

    // Apply global word wrap preference
    editor->setWrapMode(wordWrapAction->isChecked() ? QsciScintilla::WrapWord : QsciScintilla::WrapNone);

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
    editor->setFocus();

    connect(editor, &CodeEditor::modificationChanged, [this, editor]() {
        // Find the index of this editor in case it moved
        int idx = tabWidget->indexOf(editor);
        if (idx != -1) {
            updateTabTitle(idx);
        }
        if (editor == currentEditor()) {
            updateStatusBar();
        }
    });

    connect(editor, &CodeEditor::cursorPositionChanged, [this, editor](int, int) {
        if (editor == currentEditor()) {
            updateStatusBar();
        }
    });

    updateStatusBar();
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

        addToRecentClosedFiles(filePath);
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

void MainWindow::addToRecentClosedFiles(const QString &filePath)
{
    if (filePath.isEmpty()) return;

    // Reload from disk first: another running instance may have written
    // entries since we last loaded, and we must not clobber them.
    QStringList current = SessionManager::loadRecentClosedFiles();
    current.removeAll(filePath);
    current.prepend(filePath);
    while (current.size() > 10) {
        current.removeLast();
    }

    recentClosedFiles = current;
    SessionManager::saveRecentClosedFiles(recentClosedFiles);
}

void MainWindow::updateRecentFilesMenu()
{
    // Reload from disk in case another running instance closed files too.
    recentClosedFiles = SessionManager::loadRecentClosedFiles();

    // Remove previously created actions
    for (QAction *action : recentFileActions) {
        fileMenu->removeAction(action);
        action->deleteLater();
    }
    recentFileActions.clear();

    // Determine which files are currently open in tabs
    QSet<QString> openFiles;
    for (int i = 0; i < tabWidget->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(tabWidget->widget(i));
        if (editor) {
            QString path = editor->property("filePath").toString();
            if (!path.isEmpty()) openFiles.insert(path);
        }
    }

    // Prune files that are open, deleted, moved or otherwise inaccessible
    QStringList validFiles;
    for (const QString &path : recentClosedFiles) {
        QFileInfo info(path);
        if (openFiles.contains(path)) continue;
        if (!info.exists() || !info.isFile() || !info.isReadable()) continue;
        validFiles.append(path);
        if (validFiles.size() >= 10) break;
    }

    if (validFiles.size() != recentClosedFiles.size()) {
        recentClosedFiles = validFiles;
        SessionManager::saveRecentClosedFiles(recentClosedFiles);
    }

    bool hasRecent = !validFiles.isEmpty();
    recentFilesSeparatorTop->setVisible(hasRecent);
    recentFilesSeparatorBottom->setVisible(hasRecent);

    for (const QString &path : validFiles) {
        QAction *action = new QAction(QFileInfo(path).fileName(), fileMenu);
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, [this, path]() {
            addEditorTab(path);
        });
        fileMenu->insertAction(recentFilesSeparatorBottom, action);
        recentFileActions.append(action);
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
    updateStatusBar();
}

void MainWindow::toggleWordWrap(bool checked)
{
    // Persist global preference
    SessionManager::saveWordWrapEnabled(checked);

    for (int i = 0; i < tabWidget->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(tabWidget->widget(i));
        if (editor) {
            editor->setWrapMode(checked ? QsciScintilla::WrapWord : QsciScintilla::WrapNone);
        }
    }
}

CodeEditor* MainWindow::currentEditor() const
{
    return qobject_cast<CodeEditor*>(tabWidget->currentWidget());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Make sure a transparency change made just before closing is persisted
    // even if the debounce timer has not fired yet.
    if (transparencyTimer && transparencyTimer->isActive()) {
        transparencyTimer->stop();
        SessionManager::saveWindowOpacity(windowOpacity);
    }

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
            // Skip tabs for files that were moved/deleted/renamed AND have no unsaved content
            if (!state.filePath.isEmpty() && !QFile::exists(state.filePath) && state.unsavedContent.isEmpty() && !state.isModified) {
                continue;
            }
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
        updateStatusBar();
    }
}

void MainWindow::updateStatusBar()
{
    CodeEditor *editor = currentEditor();
    if (!editor) {
        statusPositionLabel->clear();
        statusLanguageLabel->clear();
        statusEncodingLabel->clear();
        statusModifiedLabel->clear();
        return;
    }

    int line = 0, index = 0;
    editor->getCursorPosition(&line, &index);
    statusPositionLabel->setText(tr("Ln %1, Col %2").arg(line + 1).arg(index + 1));

    static const QMap<QString, QString> languageNames = {
        {"cpp", "C/C++"}, {"html", "HTML"}, {"css", "CSS"},
        {"javascript", "JavaScript"}, {"json", "JSON"}, {"python", "Python"},
        {"xml", "XML"}, {"bash", "Bash"}, {"sql", "SQL"}
    };
    QString lang = editor->currentLanguage();
    statusLanguageLabel->setText(languageNames.value(lang, tr("Texto Simples")));

    statusEncodingLabel->setText(tr("UTF-8"));

    statusModifiedLabel->setText(editor->isModified() ? tr("Modificado") : tr("Salvo"));
}

void MainWindow::createMenus()
{
    // File Menu
    fileMenu = menuBar()->addMenu(tr("&Arquivo"));
    fileMenu->addAction(newAct);
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    fileMenu->addAction(saveAsAct);
    fileMenu->addSeparator();
    fileMenu->addAction(closeTabAct);

    // Recently closed files section (populated dynamically on show)
    recentFilesSeparatorTop = fileMenu->addSeparator();
    recentFilesSeparatorBottom = fileMenu->addSeparator();
    recentFilesSeparatorTop->setVisible(false);
    recentFilesSeparatorBottom->setVisible(false);
    connect(fileMenu, &QMenu::aboutToShow, this, &MainWindow::updateRecentFilesMenu);

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
    editMenu->addSeparator();

    editMenu->addAction(transparencyAct);

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
    QList<QPair<QString, QString>> languages = {
        {"Plain Text", ""}, {"C/C++", "cpp"}, {"HTML", "html"}, {"CSS", "css"},
        {"JavaScript", "javascript"}, {"JSON", "json"}, {"Python", "python"},
        {"XML", "xml"}, {"Bash", "bash"}, {"SQL", "sql"}
    };
    for (const auto &lang : languages) {
        QString display = lang.first;
        QString value = lang.second;
        langMenu->addAction(display, [this, value]() {
            setLanguage(value);
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

void MainWindow::showTransparencyDialog()
{
    const int percent = qRound((1.0 - windowOpacity) * 100.0);
    TransparencyDialog *dialog = new TransparencyDialog(percent, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &TransparencyDialog::transparencyChanged, this, &MainWindow::setTransparencyPercent);

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::setTransparencyPercent(int percent)
{
    // The dialog never goes above 90%, but clamp here as well.
    percent = qBound(0, percent, 90);
    windowOpacity = 1.0 - (percent / 100.0);

    // Coalesce the many valueChanged signals emitted while dragging the slider.
    transparencyTimer->start();
}

void MainWindow::applyTransparency()
{
    applyWindowOpacity(windowOpacity);
    SessionManager::saveWindowOpacity(windowOpacity);
}

void MainWindow::applyWindowOpacity(qreal opacity)
{
    if (QGuiApplication::platformName().contains(QLatin1String("wayland"), Qt::CaseInsensitive)) {
        // Qt's Wayland platform plugin has no window-opacity support, so on
        // Plasma the value is applied through KWin's scripting API instead.
        if (isKdeWaylandSession()) {
            applyOpacityViaKWin(opacity);
        }
        return;
    }

    setWindowOpacity(opacity);
}

void MainWindow::applyOpacityViaKWin(qreal opacity)
{
    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }

    static int scriptCounter = 0;
    const QString pluginName = QStringLiteral("simple-editor-opacity-%1").arg(++scriptCounter);
    const QString scriptPath = QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation))
                                   .filePath(pluginName + QStringLiteral(".js"));

    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    scriptFile.write(QStringLiteral(R"JS(
(function () {
    const opacity = %1;
    const windows = workspace.windowList ? workspace.windowList() : workspace.clientList();
    for (const window of windows) {
        const desktopFile = String(window.desktopFileName || '');
        const resourceClass = String(window.resourceClass || '').toLowerCase();
        const caption = String(window.caption || '');
        if (desktopFile !== 'SimpleEditor' && resourceClass.indexOf('simpleeditor') === -1)
            continue;
        // Only the main window: dialogs stay opaque so the setting can always
        // be changed back, even at the maximum transparency.
        if (caption.indexOf('Simple C++ Editor') === -1)
            continue;
        window.opacity = opacity;
    }
})();
)JS").arg(QString::number(opacity, 'f', 3)).toUtf8());
    scriptFile.close();

    const QString service = QStringLiteral("org.kde.KWin");
    const QString path = QStringLiteral("/Scripting");
    const QString iface = QStringLiteral("org.kde.kwin.Scripting");

    QDBusMessage load = QDBusMessage::createMethodCall(service, path, iface, QStringLiteral("loadScript"));
    load << scriptPath << pluginName;
    const QDBusMessage loadReply = bus.call(load, QDBus::Block, 250);

    if (loadReply.type() == QDBusMessage::ErrorMessage) {
        QFile::remove(scriptPath);
        return;
    }

    bus.call(QDBusMessage::createMethodCall(service, path, iface, QStringLiteral("start")),
             QDBus::Block, 250);

    QDBusMessage unload = QDBusMessage::createMethodCall(service, path, iface, QStringLiteral("unloadScript"));
    unload << pluginName;
    bus.call(unload, QDBus::Block, 250);

    QFile::remove(scriptPath);
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
        
        statusBar()->showMessage(found ? QString() : tr("Nenhuma ocorrência encontrada."), found ? 0 : 3000);
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
        QString term = searchDialog->getSearchText();
        if (term.isEmpty()) return;

        // QSciScintilla::replace() only works if the selection was found via findFirst/findNext.
        // We try to "find" the current selection to mark it as found if it matches the term.
        if (editor->hasSelectedText()) {
            int lineFrom, indexFrom, lineTo, indexTo;
            editor->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
            
            bool isMatch = editor->findFirst(term,
                                            searchDialog->isRegex(),
                                            searchDialog->isCaseSensitive(),
                                            searchDialog->isWholeWord(),
                                            false, // no wrap
                                            true,  // forward
                                            lineFrom,
                                            indexFrom,
                                            false); // don't show yet
            
            if (isMatch) {
                int fLineFrom, fIndexFrom, fLineTo, fIndexTo;
                editor->getSelection(&fLineFrom, &fIndexFrom, &fLineTo, &fIndexTo);
                if (fLineFrom == lineFrom && fIndexFrom == indexFrom &&
                    fLineTo == lineTo && fIndexTo == indexTo) {
                    editor->replace(searchDialog->getReplaceText());
                } else {
                    // findFirst moved the selection to the next match, restore it
                    // so that doFindNext() can start from the correct position.
                    editor->setSelection(lineFrom, indexFrom, lineTo, indexTo);
                }
            }
        } else {
            // If no selection, just find the next one
            doFindNext();
            return;
        }
        
        doFindNext();
    }
}

void MainWindow::doReplaceAll()
{
    CodeEditor *editor = currentEditor();
    if (editor && searchDialog) {
        QString term = searchDialog->getSearchText();
        if (term.isEmpty()) return;

        // Initialize search from the beginning
        bool found = editor->findFirst(term,
                                       searchDialog->isRegex(),
                                       searchDialog->isCaseSensitive(),
                                       searchDialog->isWholeWord(),
                                       false, // no wrap
                                       true,  // forward
                                       0, 0,
                                       false);
        
        if (found) {
            // Group all replacements into a single undo step so Ctrl+Z reverts
            // the whole "Replace All" instead of one match at a time.
            editor->beginUndoAction();
            editor->replace(searchDialog->getReplaceText());
            while (editor->findNext()) {
                editor->replace(searchDialog->getReplaceText());
            }
            editor->endUndoAction();
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
