#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QStack>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>
#include "codeeditor.h"
#include "sessionmanager.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void addEditorTab(const QString &filePath = QString(), const QString &content = QString(), bool isModified = false);
    void handleMessage(const QString &message);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    void closeTab(int index);
    void updateTabTitle();
    void onTabChanged(int index);
    void reopenLastTab();
    void newLocalConnection();
    void toggleWordWrap(bool checked);

private:
    void createToolBar();
    void setupTabs();
    CodeEditor* currentEditor() const;
    void loadSession();
    void saveSession();

    QTabWidget *tabWidget;
    QStack<TabState> closedTabsStack;
    QLocalServer *localServer;
    QAction *wordWrapAction;
};

#endif // MAINWINDOW_H
