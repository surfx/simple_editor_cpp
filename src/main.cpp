#include <QApplication>
#include <QFileInfo>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QtNetwork/QLocalSocket>
#include "mainwindow.h"

namespace {
const char *kServerName = "SimpleEditorServer";

// Monta o pedido enviado para a instância já em execução. Além dos arquivos,
// envia o token de ativação do Wayland (xdg-activation) herdado do lançador
// (Dolphin, por exemplo). É esse token que permite que o compositor entregue
// o foco para a janela existente quando o usuário escolhe "Abrir com".
QByteArray buildOpenRequest(const QStringList &files)
{
    QJsonObject request;
    request.insert(QStringLiteral("files"), QJsonArray::fromStringList(files));
    request.insert(QStringLiteral("activationToken"),
                   QString::fromUtf8(qgetenv("XDG_ACTIVATION_TOKEN")));
    return QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
}
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Identidade fixa do aplicativo: o app_id no Wayland (e o WM_CLASS no X11)
    // passa a ser sempre "SimpleEditor", independentemente do nome usado para
    // chamar o binário (SimpleEditor, editor, ...). Isso é o que faz o
    // gerenciador de tarefas casar a janela com SimpleEditor.desktop.
    QCoreApplication::setApplicationName(QStringLiteral("SimpleEditor"));
    QGuiApplication::setDesktopFileName(QStringLiteral("SimpleEditor"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("text-x-generic")));

    // Theme Application (Tokyo Night)
    a.setStyleSheet(
        "QMainWindow, QDialog { background-color: #1a1b26; color: #c0caf5; }"
        "QMenuBar { background-color: #1a1b26; color: #c0caf5; border-bottom: 1px solid #2f3549; }"
        "QMenuBar::item:selected { background-color: #2f3549; }"
        "QMenu { background-color: #1a1b26; color: #c0caf5; border: 1px solid #2f3549; }"
        "QMenu::item:selected { background-color: #2f3549; }"
        "QToolBar { background-color: #1a1b26; border-bottom: 1px solid #2f3549; spacing: 5px; padding: 2px; }"
        "QStatusBar { background-color: #1a1b26; color: #565f89; border-top: 1px solid #2f3549; }"
        "QTabWidget::pane { border: 1px solid #2f3549; background-color: #24283b; }"
        "QTabBar::tab { background-color: #1a1b26; color: #565f89; padding: 8px 12px; border: 1px solid #2f3549; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background-color: #24283b; color: #c0caf5; border-bottom: none; }"
        "QTabBar::tab:hover { background-color: #2f3549; }"
        "QPushButton { background-color: #2f3549; color: #c0caf5; border: 1px solid #414868; padding: 5px 15px; border-radius: 3px; }"
        "QPushButton:hover { background-color: #3b4261; }"
        "QPushButton:pressed { background-color: #414868; }"
        "QLineEdit, QComboBox { background-color: #24283b; color: #c0caf5; border: 1px solid #2f3549; padding: 4px; border-radius: 3px; }"
    );

    // Arquivos passados na linha de comando (ex: "editor arquivo.txt")
    QStringList files;
    const QStringList args = a.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (!args[i].isEmpty() && !args[i].startsWith(QLatin1Char('-'))) {
            files << QFileInfo(args[i]).absoluteFilePath();
        }
    }

    // Check if another instance is running
    QLocalSocket socket;
    socket.connectToServer(kServerName);
    if (socket.waitForConnected(500)) {
        // Another instance is running. Hand the files over and exit.
        socket.write(buildOpenRequest(files));
        socket.waitForBytesWritten(1000);
        socket.disconnectFromServer();
        return 0;
    }

    // No other instance running. Start as normal.
    MainWindow w;
    w.show();

    // Process initial command line arguments
    for (const QString &filePath : files) {
        w.addEditorTab(filePath);
    }

    return a.exec();
}
