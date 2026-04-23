#pragma once
#include <QObject>
#include <QImage>
#include <QThread>
#include <QString>
#include <QStringList>
#include <atomic>
#include <mutex>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/tracking.hpp>
#include "yoloassist.h"

class CameraWorker : public QObject {
Q_OBJECT

public:
    explicit CameraWorker(QObject* parent = nullptr);
    ~CameraWorker();

    void start();
    void stop();
    void requestStop();
    void setCameraPipeline(const QString& pipeline);
    void setVideoFile(const QString& filePath);
    void setYoloModel(const QString& modelPath, const QStringList& classNames);
    void setAiEnabled(bool enabled);
    void setAiInterval(int frameInterval);
    void setDebugEnabled(bool enabled);

signals:
    void frameReady(const QImage& frame);
    void debugInfoReady(const QString& info);
    void videoInfo(int totalFrames, double fps);
    void positionChanged(int frameIndex);
    void playbackEnded();
    void trackerStateChanged(bool active);
    void finished();

public slots:
    void run();
    void setPaused(bool paused);
    void setPlaybackSpeed(double speed);
    void seekToFrame(int frameIndex);
    void setTrackerTypes(const QStringList& trackerTypes);

    void initTracker(int x, int y, int width, int height);
    void resetTracker();

private:
    cv::Ptr<cv::Tracker> createTrackerByName(const QString& trackerType) const;
    void rebuildTrackersLocked();
    void applyTrackerBoxLocked(const cv::Rect2d& box, const cv::Mat& frame);

    QString m_source;
    bool m_useGstreamer{true};
    bool m_isVideoFile{false};
    cv::VideoCapture m_cap;
    std::atomic_bool m_running{false};
    std::atomic_bool m_paused{false};
    std::atomic_int m_seekFrame{-1};
    std::atomic<double> m_playbackSpeed{1.0};

    std::mutex m_trackerMutex;
    QStringList m_trackerTypes{"CSRT"};
    std::vector<cv::Ptr<cv::Tracker>> m_trackers;
    bool m_trackerActive{false};
    bool m_trackerInitialized{false};
    cv::Rect2d m_trackerBox;

    std::mutex m_yoloMutex;
    QString m_yoloModelPath;
    QStringList m_yoloClassNames;
    YoloAssist m_yoloAssist;
    bool m_aiEnabled{false};
    int m_aiIntervalFrames{30};
    int m_targetClassId{-1};
    bool m_targetClassPending{false};
    std::atomic_bool m_debugEnabled{false};
};
