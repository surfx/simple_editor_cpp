#include <QApplication>
#include <QtNetwork/QLocalSocket>
#include <QFileInfo>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
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
