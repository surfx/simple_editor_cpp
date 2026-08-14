#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QStack>
#include <QFileSystemWatcher>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>
#include "codeeditor.h"
#include "sessionmanager.h"
#include "searchdialog.h"
#include "themedialog.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void addEditorTab(const QString &filePath = QString(), const QString &content = QString(), bool isModified = false, const QString &language = QString());
    void handleMessage(const QString &message);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void newFile();
    void openFile();
    bool saveFile(bool forcePathCheck = true);
    bool saveFileAs();
    void closeTab(int index);
    void updateTabTitle(int index = -1);
    void onTabChanged(int index);
    void reopenLastTab();
    void newLocalConnection();
    void toggleWordWrap(bool checked);
    void showThemeDialog();
    void applyTheme(int index);
    void handleFileChanged(const QString &path);
    void reloadCurrentFile();
    void setLanguage(const QString &lang);
    void hideNotification();

    // Search and Replace
    void showFindDialog();
    void showReplaceDialog();
    void doFindNext();
    void doFindPrevious();
    void doReplace();
    void doReplaceAll();

    // Text manipulation
    void duplicateLine();
    void deleteLine();
    void moveLineUp();
    void moveLineDown();
    void toggleComment();
    void indentSelection();
    void unindentSelection();

private:
    void createActions();
    void createToolBar();
    void createMenus();
    CodeEditor* currentEditor() const;
    void loadSession();
    void saveSession();

    QTabWidget *tabWidget;
    QStack<TabState> closedTabsStack;
    QLocalServer *localServer;
    QAction *wordWrapAction;
    QFileSystemWatcher *fileWatcher;
    
    // Shared Actions
    QAction *newAct;
    QAction *openAct;
    QAction *saveAct;
    QAction *saveAsAct;
    QAction *undoAct;
    QAction *redoAct;
    QAction *cutAct;
    QAction *copyAct;
    QAction *pasteAct;
    QAction *selectAllAct;
    QAction *duplicateLineAct;
    QAction *deleteLineAct;
    QAction *moveLineUpAct;
    QAction *moveLineDownAct;
    QAction *toggleCommentAct;
    QAction *indentAct;
    QAction *unindentAct;
    QAction *findAct;
    QAction *replaceAct;
    QAction *findNextAct;
    QAction *findPrevAct;
    QAction *reopenAct;
    QAction *changeThemeAct;
    QAction *exitAct;

    SearchDialog *searchDialog = nullptr;
    int currentThemeIndex = 0;
    int normalWidth = 800;
    int normalHeight = 600;

    // Notification Bar
    QWidget *notificationBar;
    QLabel *notificationLabel;
    QPushButton *reloadButton;
    QMap<QString, bool> externallyModifiedFiles;
};

#endif // MAINWINDOW_H
