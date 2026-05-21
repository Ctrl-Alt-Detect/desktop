#include "application_settings.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>

ApplicationSettings::ApplicationSettings(QObject* parent)
    : QObject(parent),
      m_trackerTypes({"CSRT"}),
      m_cameraResolution(640, 480),
      m_aiEnabled(false),
      m_aiInterval(30),
      m_playbackSpeed(100) {
}

ApplicationSettings::~ApplicationSettings() {
}

void ApplicationSettings::setTrackerTypes(const QStringList& types) {
    if (m_trackerTypes != types) {
        m_trackerTypes = types;
        emit trackerTypesChanged(m_trackerTypes);
    }
}

void ApplicationSettings::setCameraResolution(const QSize& size) {
    if (m_cameraResolution != size) {
        m_cameraResolution = size;
        emit cameraResolutionChanged(m_cameraResolution);
    }
}

void ApplicationSettings::setAiEnabled(bool enabled) {
    if (m_aiEnabled != enabled) {
        m_aiEnabled = enabled;
        emit aiEnabledChanged(m_aiEnabled);
    }
}

void ApplicationSettings::setAiInterval(int intervalFrames) {
    if (m_aiInterval != intervalFrames) {
        m_aiInterval = std::max(1, intervalFrames);
        emit aiIntervalChanged(m_aiInterval);
    }
}

void ApplicationSettings::setPlaybackSpeed(int speed) {
    int clamped = std::max(25, std::min(300, speed));
    if (m_playbackSpeed != clamped) {
        m_playbackSpeed = clamped;
        emit playbackSpeedChanged(m_playbackSpeed);
    }
}

bool ApplicationSettings::saveToFile(const QString& filePath) {
    QJsonObject root;
    root["version"] = 1;

    // Tracker types
    QJsonArray trackerArray;
    for (const QString& tracker : m_trackerTypes) {
        trackerArray.append(tracker);
    }
    root["trackerTypes"] = trackerArray;

    // Camera resolution
    QJsonObject resObj;
    resObj["width"] = m_cameraResolution.width();
    resObj["height"] = m_cameraResolution.height();
    root["cameraResolution"] = resObj;

    // AI settings
    root["aiEnabled"] = m_aiEnabled;
    root["aiInterval"] = m_aiInterval;

    // Playback
    root["playbackSpeed"] = m_playbackSpeed;

    QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open settings file for writing:" << filePath;
        return false;
    }

    file.write(doc.toJson());
    file.close();
    return true;
}

bool ApplicationSettings::loadFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open settings file for reading:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull()) {
        qWarning() << "Failed to parse settings JSON:" << parseError.errorString();
        return false;
    }

    QJsonObject root = doc.object();
    int version = root["version"].toInt(1);
    if (version != 1) {
        qWarning() << "Unsupported settings version:" << version;
        return false;
    }

    // Tracker types
    QJsonArray trackerArray = root["trackerTypes"].toArray();
    QStringList trackerList;
    for (const QJsonValue& val : trackerArray) {
        trackerList.append(val.toString());
    }
    if (!trackerList.isEmpty()) {
        setTrackerTypes(trackerList);
    }

    // Camera resolution
    QJsonObject resObj = root["cameraResolution"].toObject();
    if (!resObj.isEmpty()) {
        QSize res(resObj["width"].toInt(640), resObj["height"].toInt(480));
        setCameraResolution(res);
    }

    // AI settings
    if (root.contains("aiEnabled")) {
        setAiEnabled(root["aiEnabled"].toBool());
    }
    if (root.contains("aiInterval")) {
        setAiInterval(root["aiInterval"].toInt(30));
    }

    // Playback
    if (root.contains("playbackSpeed")) {
        setPlaybackSpeed(root["playbackSpeed"].toInt(100));
    }

    return true;
}

QString ApplicationSettings::defaultConfigPath() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir(configDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return configDir + "/settings.json";
}
