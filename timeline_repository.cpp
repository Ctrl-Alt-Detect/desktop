#include "timeline_repository.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>

TimelineRepository::TimelineRepository(QObject* parent)
    : QObject(parent) {
}

TimelineRepository::~TimelineRepository() {
    m_events.clear();
}

void TimelineRepository::addEvent(int frameIndex, const TrackingEvent& event) {
    m_events[frameIndex] = event;
    emit eventAdded(frameIndex);
}

void TimelineRepository::removeEvent(int frameIndex) {
    if (m_events.remove(frameIndex) > 0) {
        emit eventRemoved(frameIndex);
    }
}

void TimelineRepository::clearAll() {
    m_events.clear();
    emit eventsCleared();
}

const TrackingEvent* TimelineRepository::event(int frameIndex) const {
    auto it = m_events.find(frameIndex);
    if (it != m_events.end()) {
        return &it.value();
    }
    return nullptr;
}

bool TimelineRepository::saveToFile(const QString& filePath) {
    QJsonObject root;
    root["version"] = 1;

    QJsonArray eventsArray;
    for (auto it = m_events.constBegin(); it != m_events.constEnd(); ++it) {
        int frameIndex = it.key();
        const TrackingEvent& evt = it.value();

        QJsonObject eventObj;
        eventObj["frameIndex"] = frameIndex;
        eventObj["type"] = (evt.type == TrackingEventType::SetRoi) ? "SetRoi" : "Stop";

        if (evt.type == TrackingEventType::SetRoi) {
            QJsonObject roiObj;
            roiObj["x"] = evt.roi.x();
            roiObj["y"] = evt.roi.y();
            roiObj["width"] = evt.roi.width();
            roiObj["height"] = evt.roi.height();
            eventObj["roi"] = roiObj;
        }

        eventsArray.append(eventObj);
    }
    root["events"] = eventsArray;

    QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open timeline file for writing:" << filePath;
        return false;
    }

    file.write(doc.toJson());
    file.close();
    return true;
}

bool TimelineRepository::loadFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open timeline file for reading:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull()) {
        qWarning() << "Failed to parse timeline JSON:" << parseError.errorString();
        return false;
    }

    QJsonObject root = doc.object();
    int version = root["version"].toInt(1);
    if (version != 1) {
        qWarning() << "Unsupported timeline version:" << version;
        return false;
    }

    m_events.clear();
    QJsonArray eventsArray = root["events"].toArray();
    for (const QJsonValue& val : eventsArray) {
        QJsonObject eventObj = val.toObject();
        int frameIndex = eventObj["frameIndex"].toInt();
        QString typeStr = eventObj["type"].toString();

        TrackingEvent evt;
        if (typeStr == "SetRoi") {
            evt.type = TrackingEventType::SetRoi;
            QJsonObject roiObj = eventObj["roi"].toObject();
            evt.roi = QRect(roiObj["x"].toInt(), roiObj["y"].toInt(),
                           roiObj["width"].toInt(), roiObj["height"].toInt());
        } else if (typeStr == "Stop") {
            evt.type = TrackingEventType::Stop;
        }

        m_events[frameIndex] = evt;
    }

    return true;
}
