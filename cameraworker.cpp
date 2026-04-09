#include "cameraworker.h"
#include <QDebug>

CameraWorker::CameraWorker(QObject* parent) : QObject(parent) {}
CameraWorker::~CameraWorker() { stop(); }

void CameraWorker::start() {
    if (m_running) return;
    m_running = true;
}

void CameraWorker::stop() {
    m_running = false;
    if (m_cap.isOpened()) {
        m_cap.release();
    }
}

void CameraWorker::requestStop() {
    m_running = false;
}

void CameraWorker::run() {
    m_running = true;

    QString pipeline = "mfvideosrc ! videoconvert ! video/x-raw,format=BGR ! appsink";
    if (!m_cap.open(pipeline.toStdString(), cv::CAP_GSTREAMER)) {
        qCritical() << "Failed to open GStreamer pipeline via OpenCV";
        m_running = false;
        emit finished();
        return;
    }

    cv::Mat frame;
    while (m_running) {
        if (!m_cap.read(frame) || frame.empty()) {
            qWarning() << "Failed to grab frame";
            continue;
        }

        QImage img(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_BGR888);
        emit frameReady(img.copy());
    }

    if (m_cap.isOpened()) {
        m_cap.release();
    }
    emit finished();
}
