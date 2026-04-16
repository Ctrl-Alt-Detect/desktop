#include "cameraworker.h"
#include <QDebug>
#include <QThread>
#include <algorithm>

bool CameraWorker::loadYoloNetLocked() {
    if (m_yoloNetLoaded && !m_yoloNetDirty) {
        return true;
    }

    if (m_yoloConfigPath.isEmpty() || m_yoloWeightsPath.isEmpty()) {
        m_yoloNetLoaded = false;
        m_yoloNetDirty = false;
        return false;
    }

    try {
        m_yoloNet = cv::dnn::readNetFromDarknet(m_yoloConfigPath.toStdString(), m_yoloWeightsPath.toStdString());
        m_yoloNet.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_yoloNet.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        m_yoloNetLoaded = true;
        m_yoloNetDirty = false;
        return true;
    } catch (const cv::Exception& ex) {
        qWarning() << "Failed to load YOLO model:" << ex.what();
    }

    m_yoloNetLoaded = false;
    m_yoloNetDirty = false;
    return false;
}

bool CameraWorker::detectYoloObjects(const cv::Mat& frame, std::vector<YoloDetection>& detections) {
    detections.clear();

    std::lock_guard<std::mutex> lock(m_yoloMutex);
    if (!m_aiEnabled || m_yoloConfigPath.isEmpty() || m_yoloWeightsPath.isEmpty()) {
        return false;
    }

    if (!loadYoloNetLocked()) {
        return false;
    }

    cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0, cv::Size(416, 416), cv::Scalar(), true, false);
    m_yoloNet.setInput(blob);

    std::vector<cv::Mat> outputs;
    m_yoloNet.forward(outputs, m_yoloNet.getUnconnectedOutLayersNames());

    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;
    std::vector<int> classIds;

    for (const cv::Mat& output : outputs) {
        for (int rowIndex = 0; rowIndex < output.rows; ++rowIndex) {
            const float* row = output.ptr<float>(rowIndex);
            if (output.cols <= 5) {
                continue;
            }

            float bestClassConfidence = 0.0f;
            int bestClassId = -1;
            for (int classIndex = 5; classIndex < output.cols; ++classIndex) {
                if (row[classIndex] > bestClassConfidence) {
                    bestClassConfidence = row[classIndex];
                    bestClassId = classIndex - 5;
                }
            }

            const float objectConfidence = row[4];
            const float confidence = objectConfidence * bestClassConfidence;
            if (confidence < 0.4f || bestClassId < 0) {
                continue;
            }

            const int centerX = static_cast<int>(row[0] * frame.cols);
            const int centerY = static_cast<int>(row[1] * frame.rows);
            const int width = static_cast<int>(row[2] * frame.cols);
            const int height = static_cast<int>(row[3] * frame.rows);
            const int left = centerX - width / 2;
            const int top = centerY - height / 2;

            boxes.push_back(cv::Rect(left, top, width, height));
            confidences.push_back(confidence);
            classIds.push_back(bestClassId);
        }
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, 0.4f, 0.45f, indices);

    for (int index : indices) {
        YoloDetection detection;
        detection.classId = classIds[index];
        detection.confidence = confidences[index];
        detection.box = boxes[index];
        detections.push_back(detection);
    }

    return !detections.empty();
}

double CameraWorker::rectIntersectionOverUnion(const cv::Rect2d& lhs, const cv::Rect2d& rhs) {
    const double intersectionLeft = std::max(lhs.x, rhs.x);
    const double intersectionTop = std::max(lhs.y, rhs.y);
    const double intersectionRight = std::min(lhs.x + lhs.width, rhs.x + rhs.width);
    const double intersectionBottom = std::min(lhs.y + lhs.height, rhs.y + rhs.height);

    const double intersectionWidth = std::max(0.0, intersectionRight - intersectionLeft);
    const double intersectionHeight = std::max(0.0, intersectionBottom - intersectionTop);
    const double intersectionArea = intersectionWidth * intersectionHeight;
    const double lhsArea = std::max(0.0, lhs.width) * std::max(0.0, lhs.height);
    const double rhsArea = std::max(0.0, rhs.width) * std::max(0.0, rhs.height);

    const double unionArea = lhsArea + rhsArea - intersectionArea;
    if (unionArea <= 0.0) {
        return 0.0;
    }

    return intersectionArea / unionArea;
}

bool CameraWorker::chooseDetectionForSelection(const std::vector<YoloDetection>& detections, const cv::Rect2d& referenceBox, YoloDetection& selectedDetection) const {
    double bestScore = -1.0;
    bool found = false;

    for (const YoloDetection& detection : detections) {
        const double score = rectIntersectionOverUnion(referenceBox, detection.box) + static_cast<double>(detection.confidence) * 0.1;
        if (!found || score > bestScore) {
            bestScore = score;
            selectedDetection = detection;
            found = true;
        }
    }

    return found;
}

bool CameraWorker::chooseDetectionForCorrection(const std::vector<YoloDetection>& detections, const cv::Rect2d& referenceBox, YoloDetection& selectedDetection) const {
    double bestScore = -1.0;
    bool found = false;

    for (const YoloDetection& detection : detections) {
        const double iou = rectIntersectionOverUnion(referenceBox, detection.box);
        const double score = iou * 0.7 + static_cast<double>(detection.confidence) * 0.3;
        if (!found || score > bestScore) {
            bestScore = score;
            selectedDetection = detection;
            found = true;
        }
    }

    return found;
}

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

void CameraWorker::setYoloModel(const QString& configPath, const QString& weightsPath, const QStringList& classNames) {
    std::lock_guard<std::mutex> lock(m_yoloMutex);
    const bool modelChanged = (m_yoloConfigPath != configPath) || (m_yoloWeightsPath != weightsPath) || (m_yoloClassNames != classNames);
    m_yoloConfigPath = configPath;
    m_yoloWeightsPath = weightsPath;
    m_yoloClassNames = classNames;
    if (modelChanged) {
        m_yoloNetDirty = true;
        m_yoloNetLoaded = false;
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
            modelConfigured = !m_yoloConfigPath.isEmpty() && !m_yoloWeightsPath.isEmpty();
            runAiPass = m_aiEnabled && modelConfigured && (targetClassPending || (aiIntervalFrames > 0 && frameIndex % aiIntervalFrames == 0));
        }

        if (runAiPass) {
            std::vector<YoloDetection> detections;
            if (detectYoloObjects(frame, detections) && !detections.empty()) {
                YoloDetection selectedDetection;
                bool foundDetection = false;

                cv::Rect2d currentTrackerBox;
                {
                    std::lock_guard<std::mutex> trackerLock(m_trackerMutex);
                    currentTrackerBox = m_trackerBox;
                }

                if (targetClassPending) {
                    foundDetection = chooseDetectionForSelection(detections, currentTrackerBox, selectedDetection);
                }

                if (!foundDetection && targetClassId >= 0) {
                    std::vector<YoloDetection> matchingDetections;
                    for (const YoloDetection& detection : detections) {
                        if (detection.classId == targetClassId) {
                            matchingDetections.push_back(detection);
                        }
                    }

                    if (!matchingDetections.empty()) {
                        foundDetection = chooseDetectionForCorrection(matchingDetections, currentTrackerBox, selectedDetection);
                    }
                }

                if (!foundDetection) {
                    foundDetection = chooseDetectionForCorrection(detections, currentTrackerBox, selectedDetection);
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
