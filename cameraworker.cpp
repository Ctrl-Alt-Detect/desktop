#include "cameraworker.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QThread>
#include <algorithm>

namespace {
QString detectionClassName(int classId, const QStringList& classNames) {
    if (classId >= 0 && classId < classNames.size()) {
        return classNames[classId];
    }
    return QString("id:%1").arg(classId);
}
}

void CameraWorker::applyTrackerBoxLocked(const cv::Rect2d& box, const cv::Mat& frame) {
    cv::Rect initRect = static_cast<cv::Rect>(box);
    initRect.x = std::max(0, std::min(initRect.x, std::max(0, frame.cols - 1)));
    initRect.y = std::max(0, std::min(initRect.y, std::max(0, frame.rows - 1)));
    initRect.width = std::max(1, std::min(initRect.width, std::max(1, frame.cols - initRect.x)));
    initRect.height = std::max(1, std::min(initRect.height, std::max(1, frame.rows - initRect.y)));

    cv::Rect2d adjustedBox(initRect.x, initRect.y, initRect.width, initRect.height);
    if (!m_tracker.isActive()) {
        return;
    }

    m_tracker.initialize(frame, adjustedBox);
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

void CameraWorker::setDebugEnabled(bool enabled) {
    m_debugEnabled = enabled;
    if (!enabled) {
        emit debugInfoReady(QString());
    }
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

    m_tracker.setTrackerTypes(sanitized);
    if (m_tracker.isActive()) {
        m_tracker.reset();
        m_tracker.setActive(true);
    }
}

void CameraWorker::initTracker(int x, int y, int width, int height) {
    std::lock_guard<std::mutex> lock(m_trackerMutex);
    m_tracker.setActive(true);
    m_tracker.setPendingBox(cv::Rect2d(x, y, width, height));
    emit trackerStateChanged(true);

    std::lock_guard<std::mutex> yoloLock(m_yoloMutex);
    if (m_aiEnabled) {
        m_targetClassPending = true;
        m_targetClassId = -1;
    }
}

void CameraWorker::resetTracker() {
    std::lock_guard<std::mutex> lock(m_trackerMutex);
    m_tracker.reset();
    m_tracker.setActive(false);
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
    QElapsedTimer frameClock;
    frameClock.start();
    qint64 lastFrameTimestampMs = frameClock.elapsed();
    double smoothedFps = 0.0;
    int lastSleepDelayMs = 0;
    std::vector<YoloAssist::Detection> lastYoloDetections;
    QString lastYoloError;
    int lastYoloFrame = -1;
    bool lastYoloRan = false;

    while (m_running) {
        if (m_isVideoFile) {
            const int seekFrame = m_seekFrame.exchange(-1);
            const bool hasSeekRequest = seekFrame >= 0;
            if (seekFrame >= 0) {
                m_cap.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(seekFrame));
                std::lock_guard<std::mutex> lock(m_trackerMutex);
                if (m_tracker.isActive()) {
                    m_tracker.reset();
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
            if (m_tracker.isActive()) {
                cv::Rect2d trackerBox = m_tracker.update(frame);

                cv::Rect drawRect = static_cast<cv::Rect>(trackerBox);
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
            lastYoloRan = true;
            lastYoloFrame = frameIndex;
            if (m_yoloAssist.detect(frame, detections, &yoloError) && !detections.empty()) {
                lastYoloDetections = detections;
                lastYoloError.clear();
                YoloAssist::Detection selectedDetection;
                bool foundDetection = false;

                cv::Rect2d currentTrackerBox;
                {
                    std::lock_guard<std::mutex> trackerLock(m_trackerMutex);
                    currentTrackerBox = m_tracker.box();
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
                lastYoloDetections.clear();
                lastYoloError = yoloError;
                qWarning() << "YOLO inference failed; disabling AI for this session:" << yoloError;
                std::lock_guard<std::mutex> yoloLock(m_yoloMutex);
                m_aiEnabled = false;
                m_targetClassPending = false;
            } else {
                lastYoloDetections.clear();
                lastYoloError.clear();
            }
        } else {
            lastYoloRan = false;
        }

        QImage img(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_BGR888);
        emit frameReady(img.copy());

        const qint64 nowMs = frameClock.elapsed();
        const qint64 deltaMs = std::max<qint64>(1, nowMs - lastFrameTimestampMs);
        lastFrameTimestampMs = nowMs;
        const double instantFps = 1000.0 / static_cast<double>(deltaMs);
        smoothedFps = (smoothedFps <= 0.0) ? instantFps : (smoothedFps * 0.9 + instantFps * 0.1);

        if (m_debugEnabled.load()) {
            QString modelPath;
            QStringList classNames;
            bool aiEnabled = false;
            int aiInterval = 0;
            int targetClassId = -1;
            bool targetPending = false;
            {
                std::lock_guard<std::mutex> yoloLock(m_yoloMutex);
                modelPath = m_yoloModelPath;
                classNames = m_yoloClassNames;
                aiEnabled = m_aiEnabled;
                aiInterval = m_aiIntervalFrames;
                targetClassId = m_targetClassId;
                targetPending = m_targetClassPending;
            }

            const QString modelName = modelPath.isEmpty() ? QString("none") : QFileInfo(modelPath).fileName();
            const cv::Size inputSize = m_yoloAssist.lastInputSize();
            const QStringList outputShapes = m_yoloAssist.lastOutputShapes();

            QStringList lines;
            lines << "DEBUG [F3]";
            lines << QString("Frame: %1 | FPS: %2 | Last delay: %3 ms").arg(frameIndex).arg(QString::number(smoothedFps, 'f', 1)).arg(lastSleepDelayMs);
            lines << QString("Source FPS: %1 | Playback speed: %2x").arg(QString::number(fps, 'f', 2)).arg(QString::number(m_playbackSpeed.load(), 'f', 2));
            lines << QString("AI: %1 | Interval: %2 | YOLO model: %3")
                         .arg(aiEnabled ? "ON" : "OFF")
                         .arg(aiInterval)
                         .arg(modelName);
            lines << QString("Target class: %1 | Pending lock: %2")
                         .arg(targetClassId >= 0 ? detectionClassName(targetClassId, classNames) : QString("none"))
                         .arg(targetPending ? "yes" : "no");

            if (inputSize.width > 0 && inputSize.height > 0) {
                lines << QString("YOLO input: %1x%2").arg(inputSize.width).arg(inputSize.height);
            }
            lines << QString("YOLO outputs: %1").arg(outputShapes.isEmpty() ? QString("n/a") : outputShapes.join(", "));
            lines << QString("Raw candidates: %1 | NMS detections: %2 | Last AI frame: %3")
                         .arg(m_yoloAssist.lastRawCandidateCount())
                         .arg(static_cast<int>(lastYoloDetections.size()))
                         .arg(lastYoloFrame >= 0 ? QString::number(lastYoloFrame) : QString("n/a"));

            if (!lastYoloError.isEmpty()) {
                lines << QString("YOLO error: %1").arg(lastYoloError);
            }

            if (!lastYoloDetections.empty()) {
                lines << "Detections:";
                const int maxDetectionsToShow = 16;
                const int count = std::min(static_cast<int>(lastYoloDetections.size()), maxDetectionsToShow);
                for (int i = 0; i < count; ++i) {
                    const YoloAssist::Detection& detection = lastYoloDetections[static_cast<size_t>(i)];
                    lines << QString("  #%1 %2 conf=%3 box=[%4,%5,%6,%7]")
                                 .arg(i + 1)
                                 .arg(detectionClassName(detection.classId, classNames))
                                 .arg(QString::number(detection.confidence, 'f', 3))
                                 .arg(detection.box.x)
                                 .arg(detection.box.y)
                                 .arg(detection.box.width)
                                 .arg(detection.box.height);
                }
                if (static_cast<int>(lastYoloDetections.size()) > maxDetectionsToShow) {
                    lines << QString("  ... %1 more detections").arg(static_cast<int>(lastYoloDetections.size()) - maxDetectionsToShow);
                }
            }

            emit debugInfoReady(lines.join("\n"));
        }

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
                lastSleepDelayMs = delayMs;
                QThread::msleep(static_cast<unsigned long>(delayMs));
            } else {
                lastSleepDelayMs = 0;
            }
        } else {
            lastSleepDelayMs = 0;
        }

        ++frameIndex;
    }

    if (m_cap.isOpened()) {
        m_cap.release();
    }
    emit finished();
}
