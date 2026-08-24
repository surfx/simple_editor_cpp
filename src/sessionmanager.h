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
    QString language;
};

class SessionManager {
public:
    static void saveSession(const QList<TabState>& tabs, int currentIndex, int width, int height);
    static bool loadSession(QList<TabState>& tabs, int& currentIndex, int& width, int& height);

    static void saveWordWrapEnabled(bool enabled);
    static bool loadWordWrapEnabled();

private:
    static QString getSessionFilePath();
    static QString getSettingsFilePath();
};

#endif // SESSIONMANAGER_H
