#include <QApplication>
#include <QtNetwork/QLocalSocket>
#include <QFileInfo>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
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

    // Check if another instance is running
    QLocalSocket socket;
    socket.connectToServer("SimpleEditorServer");
    if (socket.waitForConnected(500)) {
        // Another instance is running. Send file paths and exit.
        QStringList args = a.arguments();
        for (int i = 1; i < args.size(); ++i) {
            QString filePath = QFileInfo(args[i]).absoluteFilePath();
            socket.write(filePath.toUtf8());
            socket.waitForBytesWritten(500);
        }
        return 0;
    }

    // No other instance running. Start as normal.
    MainWindow w;
    w.show();
    
    // Process initial command line arguments
    QStringList args = a.arguments();
    for (int i = 1; i < args.size(); ++i) {
        QString filePath = QFileInfo(args[i]).absoluteFilePath();
        w.addEditorTab(filePath);
    }
    
    return a.exec();
}
