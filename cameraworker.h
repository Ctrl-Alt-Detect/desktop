#pragma once
#include <QObject>
#include <QImage>
#include <QThread>
#include <QString>
#include <atomic>
#include <opencv2/opencv.hpp>

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

signals:
    void frameReady(const QImage& frame);
    void videoInfo(int totalFrames, double fps);
    void positionChanged(int frameIndex);
    void finished();

public slots:
    void run();
    void setPaused(bool paused);
    void setPlaybackSpeed(double speed);
    void seekToFrame(int frameIndex);

private:
    QString m_source;
    bool m_useGstreamer{true};
    bool m_isVideoFile{false};
    cv::VideoCapture m_cap;
    std::atomic_bool m_running{false};
    std::atomic_bool m_paused{false};
    std::atomic_int m_seekFrame{-1};
    std::atomic<double> m_playbackSpeed{1.0};
};
