#include "sessionmanager.h"

QString SessionManager::getSessionFilePath() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath("session.json");
}

void SessionManager::saveSession(const QList<TabState>& tabs, int currentIndex) {
    QJsonArray tabsArray;
    for (const auto& tab : tabs) {
        QJsonObject tabObj;
        tabObj["filePath"] = tab.filePath;
        tabObj["isModified"] = tab.isModified;
        if (tab.isModified) {
            tabObj["unsavedContent"] = tab.unsavedContent;
        }
        tabsArray.append(tabObj);
    }

    QJsonObject root;
    root["tabs"] = tabsArray;
    root["currentIndex"] = currentIndex;

    QFile file(getSessionFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        file.close();
    }
}

bool SessionManager::loadSession(QList<TabState>& tabs, int& currentIndex) {
    QFile file(getSessionFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return false;

    QJsonObject root = doc.object();
    QJsonArray tabsArray = root["tabs"].toArray();
    currentIndex = root["currentIndex"].toInt();

    for (int i = 0; i < tabsArray.size(); ++i) {
        QJsonObject tabObj = tabsArray[i].toObject();
        TabState state;
        state.filePath = tabObj["filePath"].toString();
        state.isModified = tabObj["isModified"].toBool();
        if (state.isModified) {
            state.unsavedContent = tabObj["unsavedContent"].toString();
        }
        tabs.append(state);
    }

    return true;
}
