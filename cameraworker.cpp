#include "cameraworker.h"
#include <QDebug>
#include <QThread>
#include <algorithm>

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

void CameraWorker::setCameraPipeline(const QString& pipeline) {
    m_source = pipeline;
    m_useGstreamer = true;
    m_isVideoFile = false;
}

void CameraWorker::setVideoFile(const QString& filePath) {
    m_source = filePath;
    m_useGstreamer = false;
    m_isVideoFile = true;
}

void CameraWorker::setPaused(bool paused) {
    m_paused = paused;
}

void CameraWorker::setPlaybackSpeed(double speed) {
    m_playbackSpeed = std::max(0.1, speed);
}

void CameraWorker::seekToFrame(int frameIndex) {
    m_seekFrame = std::max(0, frameIndex);
}

void CameraWorker::initTracker(int x, int y, int width, int height) {
    std::lock_guard<std::mutex> lock(m_trackerMutex);
    m_trackerBox = cv::Rect2d(x, y, width, height);
    m_trackerActive = true;
    m_trackerInitialized = false;
    m_tracker = cv::TrackerCSRT::create();
    emit trackerStateChanged(true);
}

void CameraWorker::resetTracker() {
    std::lock_guard<std::mutex> lock(m_trackerMutex);
    m_tracker.release();
    m_trackerActive = false;
    m_trackerInitialized = false;
    emit trackerStateChanged(false);
}

void CameraWorker::run() {
    m_running = true;
    m_paused = false;
    m_seekFrame = -1;

    if (m_source.isEmpty()) {
        m_source = "mfvideosrc ! videoconvert ! video/x-raw,format=BGR ! appsink";
        m_useGstreamer = true;
        m_isVideoFile = false;
    }

    bool opened = false;
    if (m_useGstreamer) {
        opened = m_cap.open(m_source.toStdString(), cv::CAP_GSTREAMER);
    } else {
        opened = m_cap.open(m_source.toStdString());
    }

    if (!opened) {
        qCritical() << "Failed to open source:" << m_source;
        m_running = false;
        emit finished();
        return;
    }

    double fps = 30.0;
    int totalFrames = 0;
    if (m_isVideoFile) {
        const double sourceFps = m_cap.get(cv::CAP_PROP_FPS);
        if (sourceFps > 1.0 && sourceFps < 240.0) {
            fps = sourceFps;
        }
        totalFrames = static_cast<int>(m_cap.get(cv::CAP_PROP_FRAME_COUNT));
        emit videoInfo(std::max(0, totalFrames), fps);
    }

    cv::Mat frame;
    while (m_running) {
        if (m_isVideoFile) {
            const int seekFrame = m_seekFrame.exchange(-1);
            if (seekFrame >= 0) {
                m_cap.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(seekFrame));
            }

            if (m_paused) {
                QThread::msleep(15);
                continue;
            }
        }

        if (!m_cap.read(frame)) {
            if (m_isVideoFile) {
                qInfo() << "Reached end of video:" << m_source;
                break;
            }
            qWarning() << "Failed to grab frame";
            continue;
        }

        if (frame.empty()) {
            if (m_isVideoFile) {
                break;
            }
            continue;
        }

        // CSRT tracker-based tracking
        {
            std::lock_guard<std::mutex> lock(m_trackerMutex);
            if (m_trackerActive && m_tracker) {
                // Initialize tracker on first frame after selection
                if (!m_trackerInitialized) {
                    cv::Rect trackerRect = static_cast<cv::Rect>(m_trackerBox);
                    m_tracker->init(frame, trackerRect);
                    m_trackerInitialized = true;
                } else {
                    // Update tracker with new frame
                    cv::Rect boundingBox = static_cast<cv::Rect>(m_trackerBox);
                    m_tracker->update(frame, boundingBox);
                    m_trackerBox = boundingBox;
                }
                
                // Draw tracking bounding box
                cv::Rect drawRect = static_cast<cv::Rect>(m_trackerBox);
                drawRect.x = std::max(0, drawRect.x);
                drawRect.y = std::max(0, drawRect.y);
                drawRect.width = std::min(drawRect.width, frame.cols - drawRect.x);
                drawRect.height = std::min(drawRect.height, frame.rows - drawRect.y);
                if (drawRect.width > 0 && drawRect.height > 0) {
                    cv::rectangle(frame, drawRect, cv::Scalar(0, 255, 0), 2);
                }
            }
        }

        QImage img(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_BGR888);
        emit frameReady(img.copy());

        if (m_isVideoFile) {
            const int currentFrame = static_cast<int>(m_cap.get(cv::CAP_PROP_POS_FRAMES));
            emit positionChanged(std::max(0, currentFrame - 1));

            const double speed = std::max(0.1, m_playbackSpeed.load());
            const int delayMs = static_cast<int>(1000.0 / (fps * speed));
            if (delayMs > 0) {
                QThread::msleep(static_cast<unsigned long>(delayMs));
            }
        }
    }

    if (m_cap.isOpened()) {
        m_cap.release();
    }
    emit finished();
}
