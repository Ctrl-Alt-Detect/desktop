#pragma once
#include <QString>
#include <QMap>
#include <QRect>
#include <QObject>

enum class TrackingEventType {
    SetRoi,
    Stop
};

struct TrackingEvent {
    TrackingEventType type;
    QRect roi;
};

class TimelineRepository : public QObject {
    Q_OBJECT

public:
    explicit TimelineRepository(QObject* parent = nullptr);
    ~TimelineRepository();

    /// Add or update an event at the given frame index
    void addEvent(int frameIndex, const TrackingEvent& event);

    /// Remove event at the given frame index
    void removeEvent(int frameIndex);

    /// Clear all events
    void clearAll();

    /// Get event at frame index (returns nullptr if not found)
    const TrackingEvent* event(int frameIndex) const;

    /// Get all events (frame → event mapping)
    const QMap<int, TrackingEvent>& allEvents() const { return m_events; }

    /// Check if event exists at frame index
    bool hasEvent(int frameIndex) const { return m_events.contains(frameIndex); }

    /// Save timeline to JSON file
    bool saveToFile(const QString& filePath);

    /// Load timeline from JSON file
    bool loadFromFile(const QString& filePath);

signals:
    void eventAdded(int frameIndex);
    void eventRemoved(int frameIndex);
    void eventsCleared();

private:
    QMap<int, TrackingEvent> m_events;
};
