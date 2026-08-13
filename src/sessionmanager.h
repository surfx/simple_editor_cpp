#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QDir>
#include <QFile>

struct TabState {
    QString filePath;
    bool isModified;
    QString unsavedContent;
};

class SessionManager {
public:
    static void saveSession(const QList<TabState>& tabs, int currentIndex, int width, int height);
    static bool loadSession(QList<TabState>& tabs, int& currentIndex, int& width, int& height);

private:
    static QString getSessionFilePath();
};

#endif // SESSIONMANAGER_H
