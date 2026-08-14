#include "sessionmanager.h"

QString SessionManager::getSessionFilePath() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath("session.json");
}

void SessionManager::saveSession(const QList<TabState>& tabs, int currentIndex, int width, int height) {
    QJsonArray tabsArray;
    for (const auto& tab : tabs) {
        QJsonObject tabObj;
        tabObj["filePath"] = tab.filePath;
        tabObj["isModified"] = tab.isModified;
        tabObj["language"] = tab.language;
        if (tab.isModified) {
            tabObj["unsavedContent"] = tab.unsavedContent;
        }
        tabsArray.append(tabObj);
    }

    QJsonObject root;
    root["tabs"] = tabsArray;
    root["currentIndex"] = currentIndex;
    root["windowWidth"] = width;
    root["windowHeight"] = height;

    QFile file(getSessionFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        file.close();
    }
}

bool SessionManager::loadSession(QList<TabState>& tabs, int& currentIndex, int& width, int& height) {
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
    width = root["windowWidth"].toInt(800);
    height = root["windowHeight"].toInt(600);

    for (int i = 0; i < tabsArray.size(); ++i) {
        QJsonObject tabObj = tabsArray[i].toObject();
        TabState state;
        state.filePath = tabObj["filePath"].toString();
        state.isModified = tabObj["isModified"].toBool();
        state.language = tabObj["language"].toString();
        if (state.isModified) {
            state.unsavedContent = tabObj["unsavedContent"].toString();
        }
        tabs.append(state);
    }

    return true;
}
