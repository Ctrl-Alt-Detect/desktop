#pragma once
#include <QObject>
#include <QImage>
#include <QThread>
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

signals:
    void frameReady(const QImage& frame);
    void finished();

public slots:
    void run();

private:
    cv::VideoCapture m_cap;
    std::atomic_bool m_running{false};
};
