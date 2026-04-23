#include "cameraworker.h"
#include <QDebug>
#include <QThread>
#include <algorithm>

void CameraWorker::applyTrackerBoxLocked(const cv::Rect2d& box, const cv::Mat& frame) {
    cv::Rect initRect = static_cast<cv::Rect>(box);
    initRect.x = std::max(0, std::min(initRect.x, std::max(0, frame.cols - 1)));
    initRect.y = std::max(0, std::min(initRect.y, std::max(0, frame.rows - 1)));
    initRect.width = std::max(1, std::min(initRect.width, std::max(1, frame.cols - initRect.x)));
    initRect.height = std::max(1, std::min(initRect.height, std::max(1, frame.rows - initRect.y)));

    m_trackerBox = cv::Rect2d(initRect.x, initRect.y, initRect.width, initRect.height);
    if (!m_trackerActive) {
        return;
    }

    rebuildTrackersLocked();
    m_trackerInitialized = false;

    int initCount = 0;
    for (auto& tracker : m_trackers) {
        if (!tracker) {
            continue;
        }

        try {
            tracker->init(frame, initRect);
            ++initCount;
        } catch (const cv::Exception& ex) {
            qWarning() << "Tracker reinit failed after YOLO correction:" << ex.what();
        }
    }

    if (initCount == 0) {
        try {
            m_trackers.clear();
            cv::Ptr<cv::Tracker> fallback = cv::TrackerCSRT::create();
            fallback->init(frame, initRect);
            m_trackers.push_back(fallback);
            initCount = 1;
        } catch (const cv::Exception& ex) {
            qWarning() << "Fallback tracker init failed after YOLO correction:" << ex.what();
        }
    }

    m_trackerInitialized = (initCount > 0);
}

cv::Ptr<cv::Tracker> CameraWorker::createTrackerByName(const QString& trackerType) const {
    const QString name = trackerType.trimmed();
    if (name.compare("CSRT", Qt::CaseInsensitive) == 0) {
        return cv::TrackerCSRT::create();
    }
    if (name.compare("KCF", Qt::CaseInsensitive) == 0) {
        return cv::TrackerKCF::create();
    }
    if (name.compare("MIL", Qt::CaseInsensitive) == 0) {
        return cv::TrackerMIL::create();
    }
    if (name.compare("GOTURN", Qt::CaseInsensitive) == 0) {
        return cv::TrackerGOTURN::create();
    }
    if (name.compare("DaSiamRPN", Qt::CaseInsensitive) == 0) {
        return cv::TrackerDaSiamRPN::create();
    }
    if (name.compare("Nano", Qt::CaseInsensitive) == 0) {
        return cv::TrackerNano::create();
    }
    if (name.compare("Vit", Qt::CaseInsensitive) == 0) {
        return cv::TrackerVit::create();
    }
    return {};
}

void CameraWorker::rebuildTrackersLocked() {
    m_trackers.clear();
    for (const QString& trackerType : m_trackerTypes) {
        try {
            cv::Ptr<cv::Tracker> tracker = createTrackerByName(trackerType);
            if (tracker) {
                m_trackers.push_back(tracker);
            }
        } catch (const cv::Exception& ex) {
            qWarning() << "Failed to create tracker" << trackerType << ":" << ex.what();
        }
    }

    if (m_trackers.empty()) {
        m_trackers.push_back(cv::TrackerCSRT::create());
    }
}

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

void CameraWorker::setYoloModel(const QString& modelPath, const QStringList& classNames) {
    std::lock_guard<std::mutex> lock(m_yoloMutex);
    const bool modelChanged = (m_yoloModelPath != modelPath) || (m_yoloClassNames != classNames);
    m_yoloModelPath = modelPath;
    m_yoloClassNames = classNames;
    m_yoloAssist.setModel(modelPath, classNames);
    if (modelChanged) {
        m_targetClassId = -1;
        m_targetClassPending = false;
    }
}

void CameraWorker::setAiEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_yoloMutex);
    m_aiEnabled = enabled;
    if (!m_aiEnabled) {
        m_targetClassPending = false;
    }
}

void CameraWorker::setAiInterval(int frameInterval) {
    std::lock_guard<std::mutex> lock(m_yoloMutex);
    m_aiIntervalFrames = std::max(1, frameInterval);
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

void CameraWorker::setTrackerTypes(const QStringList& trackerTypes) {
    std::lock_guard<std::mutex> lock(m_trackerMutex);

    QStringList sanitized;
    for (const QString& trackerType : trackerTypes) {
        const QString trimmed = trackerType.trimmed();
        if (!trimmed.isEmpty() && !sanitized.contains(trimmed, Qt::CaseInsensitive)) {
            sanitized.push_back(trimmed);
        }
    }

    if (sanitized.isEmpty()) {
        sanitized.push_back("CSRT");
    }

    m_trackerTypes = sanitized;
    if (m_trackerActive) {
        m_trackerInitialized = false;
        rebuildTrackersLocked();
    }
}

void CameraWorker::initTracker(int x, int y, int width, int height) {
    std::lock_guard<std::mutex> lock(m_trackerMutex);
    m_trackerBox = cv::Rect2d(x, y, width, height);
    m_trackerActive = true;
    m_trackerInitialized = false;
    rebuildTrackersLocked();
    emit trackerStateChanged(true);

    std::lock_guard<std::mutex> yoloLock(m_yoloMutex);
    if (m_aiEnabled) {
        m_targetClassPending = true;
        m_targetClassId = -1;
    }
}

void CameraWorker::resetTracker() {
    std::lock_guard<std::mutex> lock(m_trackerMutex);
    m_trackers.clear();
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
    int frameIndex = 0;
    while (m_running) {
        if (m_isVideoFile) {
            const int seekFrame = m_seekFrame.exchange(-1);
            const bool hasSeekRequest = seekFrame >= 0;
            if (seekFrame >= 0) {
                m_cap.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(seekFrame));
                std::lock_guard<std::mutex> lock(m_trackerMutex);
                if (m_trackerActive) {
                    rebuildTrackersLocked();
                    m_trackerInitialized = false;
                }
            }

            // Keep paused playback responsive: if a seek was requested while paused,
            // process one frame so the UI immediately shows the new position.
            if (m_paused && !hasSeekRequest) {
                QThread::msleep(15);
                continue;
            }
        }

        if (!m_cap.read(frame)) {
            if (m_isVideoFile) {
                qInfo() << "Reached end of video:" << m_source;
                m_paused = true;
                m_seekFrame = 0;
                emit playbackEnded();
                QThread::msleep(15);
                continue;
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

        // Multi-tracker update: average all successful tracker boxes.
        {
            std::lock_guard<std::mutex> lock(m_trackerMutex);
            if (m_trackerActive && !m_trackers.empty()) {
                if (!m_trackerInitialized) {
                    cv::Rect trackerRect = static_cast<cv::Rect>(m_trackerBox);
                    int initCount = 0;
                    for (auto& tracker : m_trackers) {
                        if (!tracker) {
                            continue;
                        }
                        try {
                            tracker->init(frame, trackerRect);
                            ++initCount;
                        } catch (const cv::Exception& ex) {
                            qWarning() << "Tracker init failed:" << ex.what();
                        }
                    }

                    if (initCount == 0) {
                        try {
                            qWarning() << "Selected trackers failed to initialize, falling back to CSRT";
                            m_trackers.clear();
                            cv::Ptr<cv::Tracker> fallback = cv::TrackerCSRT::create();
                            fallback->init(frame, trackerRect);
                            m_trackers.push_back(fallback);
                            initCount = 1;
                        } catch (const cv::Exception& ex) {
                            qWarning() << "Fallback CSRT init failed:" << ex.what();
                        }
                    }
                    m_trackerInitialized = (initCount > 0);
                } else {
                    double sumX = 0.0;
                    double sumY = 0.0;
                    double sumWidth = 0.0;
                    double sumHeight = 0.0;
                    int successCount = 0;

                    for (auto& tracker : m_trackers) {
                        if (!tracker) {
                            continue;
                        }

                        cv::Rect trackerBox = static_cast<cv::Rect>(m_trackerBox);
                        bool updated = false;
                        try {
                            updated = tracker->update(frame, trackerBox);
                        } catch (const cv::Exception& ex) {
                            qWarning() << "Tracker update failed:" << ex.what();
                        }

                        if (updated && trackerBox.width > 0 && trackerBox.height > 0) {
                            sumX += trackerBox.x;
                            sumY += trackerBox.y;
                            sumWidth += trackerBox.width;
                            sumHeight += trackerBox.height;
                            ++successCount;
                        }
                    }

                    if (successCount > 0) {
                        m_trackerBox = cv::Rect2d(
                            sumX / successCount,
                            sumY / successCount,
                            sumWidth / successCount,
                            sumHeight / successCount);
                    }
                }

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

        bool runAiPass = false;
        int aiIntervalFrames = 0;
        int targetClassId = -1;
        bool targetClassPending = false;
        bool modelConfigured = false;
        {
            std::lock_guard<std::mutex> yoloLock(m_yoloMutex);
            aiIntervalFrames = m_aiIntervalFrames;
            targetClassId = m_targetClassId;
            targetClassPending = m_targetClassPending;
            modelConfigured = !m_yoloModelPath.isEmpty();
            runAiPass = m_aiEnabled && modelConfigured && (targetClassPending || (aiIntervalFrames > 0 && frameIndex % aiIntervalFrames == 0));
        }

        if (runAiPass) {
            std::vector<YoloAssist::Detection> detections;
            QString yoloError;
            if (m_yoloAssist.detect(frame, detections, &yoloError) && !detections.empty()) {
                YoloAssist::Detection selectedDetection;
                bool foundDetection = false;

                cv::Rect2d currentTrackerBox;
                {
                    std::lock_guard<std::mutex> trackerLock(m_trackerMutex);
                    currentTrackerBox = m_trackerBox;
                }

                if (targetClassPending) {
                    foundDetection = YoloAssist::chooseDetectionForSelection(detections, currentTrackerBox, selectedDetection);
                }

                if (!foundDetection && targetClassId >= 0) {
                    std::vector<YoloAssist::Detection> matchingDetections;
                    for (const YoloAssist::Detection& detection : detections) {
                        if (detection.classId == targetClassId) {
                            matchingDetections.push_back(detection);
                        }
                    }

                    if (!matchingDetections.empty()) {
                        foundDetection = YoloAssist::chooseDetectionForCorrection(matchingDetections, currentTrackerBox, selectedDetection);
                    }
                }

                if (!foundDetection) {
                    foundDetection = YoloAssist::chooseDetectionForCorrection(detections, currentTrackerBox, selectedDetection);
                }

                if (foundDetection) {
                    std::lock_guard<std::mutex> trackerLock(m_trackerMutex);
                    std::lock_guard<std::mutex> yoloLock(m_yoloMutex);
                    applyTrackerBoxLocked(cv::Rect2d(selectedDetection.box.x, selectedDetection.box.y, selectedDetection.box.width, selectedDetection.box.height), frame);
                    if (targetClassPending && m_targetClassPending) {
                        m_targetClassId = selectedDetection.classId;
                        m_targetClassPending = false;
                    }
                }
            } else if (!yoloError.isEmpty()) {
                qWarning() << "YOLO inference failed; disabling AI for this session:" << yoloError;
                std::lock_guard<std::mutex> yoloLock(m_yoloMutex);
                m_aiEnabled = false;
                m_targetClassPending = false;
            }
        }

        QImage img(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_BGR888);
        emit frameReady(img.copy());

        if (m_isVideoFile) {
            const int currentFrame = static_cast<int>(m_cap.get(cv::CAP_PROP_POS_FRAMES));
            emit positionChanged(std::max(0, currentFrame - 1));

            if (m_paused) {
                QThread::msleep(15);
                continue;
            }

            const double speed = std::max(0.1, m_playbackSpeed.load());
            const int delayMs = static_cast<int>(1000.0 / (fps * speed));
            if (delayMs > 0) {
                QThread::msleep(static_cast<unsigned long>(delayMs));
            }
        }

        ++frameIndex;
    }

    if (m_cap.isOpened()) {
        m_cap.release();
    }
    emit finished();
}
