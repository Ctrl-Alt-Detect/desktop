#pragma once
#include <QString>
#include <QStringList>
#include <QSize>
#include <QObject>

class ApplicationSettings : public QObject {
    Q_OBJECT

public:
    explicit ApplicationSettings(QObject* parent = nullptr);
    ~ApplicationSettings();

    /// Tracker types
    void setTrackerTypes(const QStringList& types);
    const QStringList& trackerTypes() const { return m_trackerTypes; }

    /// Camera resolution
    void setCameraResolution(const QSize& size);
    const QSize& cameraResolution() const { return m_cameraResolution; }

    /// AI enabled state
    void setAiEnabled(bool enabled);
    bool aiEnabled() const { return m_aiEnabled; }

    /// AI interval (frames)
    void setAiInterval(int intervalFrames);
    int aiInterval() const { return m_aiInterval; }

    /// Playback speed (percentage: 25-300)
    void setPlaybackSpeed(int speed);
    int playbackSpeed() const { return m_playbackSpeed; }

    /// Save all settings to config file
    bool saveToFile(const QString& filePath);

    /// Load all settings from config file
    bool loadFromFile(const QString& filePath);

    /// Get default config file path
    static QString defaultConfigPath();

signals:
    void trackerTypesChanged(const QStringList& types);
    void cameraResolutionChanged(const QSize& size);
    void aiEnabledChanged(bool enabled);
    void aiIntervalChanged(int intervalFrames);
    void playbackSpeedChanged(int speed);

private:
    QStringList m_trackerTypes{"CSRT"};
    QSize m_cameraResolution{640, 480};
    bool m_aiEnabled{false};
    int m_aiInterval{30};
    int m_playbackSpeed{100};
};
