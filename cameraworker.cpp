#include "cameraworker.h"
#include <QDebug>
#include <QFileInfo>
#include <QRegularExpression>
#include <QThread>
#include <algorithm>

namespace {
cv::Size inferYoloInputSizeFromModelPath(const QString& modelPath) {
    // Accept names like "yolo11n_480.onnx" or "model_640x640.onnx".
    const QRegularExpression pairPattern("(\\d{3,4})[xX](\\d{3,4})");
    QRegularExpressionMatch pairMatch = pairPattern.match(modelPath);
    if (pairMatch.hasMatch()) {
        const int width = pairMatch.captured(1).toInt();
        const int height = pairMatch.captured(2).toInt();
        if (width > 0 && height > 0) {
            return cv::Size(width, height);
        }
    }

    const QRegularExpression squarePattern("_(\\d{3,4})(?=\\.[^.]+$)");
    QRegularExpressionMatch squareMatch = squarePattern.match(modelPath);
    if (squareMatch.hasMatch()) {
        const int size = squareMatch.captured(1).toInt();
        if (size > 0) {
            return cv::Size(size, size);
        }
    }

    return cv::Size(640, 640);
}

bool isLikelyNormalizedBox(float cx, float cy, float width, float height) {
    return cx >= 0.0f && cy >= 0.0f && width > 0.0f && height > 0.0f && cx <= 1.5f && cy <= 1.5f && width <= 1.5f && height <= 1.5f;
}
}

bool CameraWorker::loadYoloNetLocked() {
    if (m_yoloNetLoaded && !m_yoloNetDirty) {
        return true;
    }

    if (m_yoloModelPath.isEmpty()) {
        m_yoloNetLoaded = false;
        m_yoloNetDirty = false;
        return false;
    }

    if (!QFileInfo::exists(m_yoloModelPath)) {
        qWarning() << "YOLO model file does not exist:" << m_yoloModelPath;
        m_yoloNetLoaded = false;
        m_yoloNetDirty = false;
        return false;
    }

    try {
        m_yoloNet = cv::dnn::readNetFromONNX(m_yoloModelPath.toStdString());
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
    if (!m_aiEnabled || m_yoloModelPath.isEmpty()) {
        return false;
    }

    try {
        if (!loadYoloNetLocked()) {
            return false;
        }

        const cv::Size modelInputSize = inferYoloInputSizeFromModelPath(m_yoloModelPath);
        cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0, modelInputSize, cv::Scalar(), true, false);
        m_yoloNet.setInput(blob);

        std::vector<cv::Mat> outputs;
        m_yoloNet.forward(outputs, m_yoloNet.getUnconnectedOutLayersNames());

        if (!m_yoloOutputInfoLogged) {
            QStringList shapes;
            for (const cv::Mat& output : outputs) {
                QStringList dims;
                for (int dimIndex = 0; dimIndex < output.dims; ++dimIndex) {
                    dims << QString::number(output.size[dimIndex]);
                }
                shapes << QString("[%1]").arg(dims.join('x'));
            }
            qInfo() << "YOLO outputs:" << shapes.join(", ") << "input" << modelInputSize.width << "x" << modelInputSize.height;
            m_yoloOutputInfoLogged = true;
        }

        std::vector<cv::Rect> boxes;
        std::vector<float> confidences;
        std::vector<int> classIds;

        auto appendDetectionsFrom2D = [&](const cv::Mat& predictions) {
            if (predictions.empty() || predictions.cols < 6) {
                return;
            }

            for (int rowIndex = 0; rowIndex < predictions.rows; ++rowIndex) {
                const float* row = predictions.ptr<float>(rowIndex);
                const int attributes = predictions.cols;

                int classStartIndex = 5;
                float objectness = row[4];

                if (!m_yoloClassNames.isEmpty()) {
                    if (attributes == m_yoloClassNames.size() + 4) {
                        classStartIndex = 4;
                        objectness = 1.0f;
                    } else if (attributes == m_yoloClassNames.size() + 5) {
                        classStartIndex = 5;
                    }
                } else if (attributes >= 8 && attributes != 85) {
                    // Most YOLOv8/11 exports are [x,y,w,h,cls...] without objectness.
                    classStartIndex = 4;
                    objectness = 1.0f;
                }

                if (attributes <= classStartIndex) {
                    continue;
                }

                float bestClassScore = 0.0f;
                int bestClassId = -1;
                for (int classIndex = classStartIndex; classIndex < attributes; ++classIndex) {
                    if (row[classIndex] > bestClassScore) {
                        bestClassScore = row[classIndex];
                        bestClassId = classIndex - classStartIndex;
                    }
                }

                const float confidence = objectness * bestClassScore;
                if (confidence < 0.25f || bestClassId < 0) {
                    continue;
                }

                float centerX = row[0];
                float centerY = row[1];
                float width = row[2];
                float height = row[3];

                if (isLikelyNormalizedBox(centerX, centerY, width, height)) {
                    centerX *= static_cast<float>(frame.cols);
                    centerY *= static_cast<float>(frame.rows);
                    width *= static_cast<float>(frame.cols);
                    height *= static_cast<float>(frame.rows);
                } else {
                    const float xScale = static_cast<float>(frame.cols) / static_cast<float>(std::max(1, modelInputSize.width));
                    const float yScale = static_cast<float>(frame.rows) / static_cast<float>(std::max(1, modelInputSize.height));
                    centerX *= xScale;
                    centerY *= yScale;
                    width *= xScale;
                    height *= yScale;
                }

                int left = static_cast<int>(centerX - width * 0.5f);
                int top = static_cast<int>(centerY - height * 0.5f);
                int boxWidth = static_cast<int>(width);
                int boxHeight = static_cast<int>(height);

                left = std::max(0, std::min(left, std::max(0, frame.cols - 1)));
                top = std::max(0, std::min(top, std::max(0, frame.rows - 1)));
                boxWidth = std::max(1, std::min(boxWidth, std::max(1, frame.cols - left)));
                boxHeight = std::max(1, std::min(boxHeight, std::max(1, frame.rows - top)));

                boxes.emplace_back(left, top, boxWidth, boxHeight);
                confidences.push_back(confidence);
                classIds.push_back(bestClassId);
            }
        };

        for (const cv::Mat& output : outputs) {
            if (output.empty()) {
                continue;
            }

            if (output.dims == 2) {
                appendDetectionsFrom2D(output);
                continue;
            }

            if (output.dims == 3 && output.size[0] == 1) {
                const int dim1 = output.size[1];
                const int dim2 = output.size[2];

                if (dim1 > 0 && dim2 > 0) {
                    cv::Mat view;
                    if (dim1 >= dim2) {
                        // [1, num_predictions, attributes]
                        view = cv::Mat(dim1, dim2, CV_32F, const_cast<float*>(output.ptr<float>(0)));
                    } else {
                        // [1, attributes, num_predictions] -> transpose to [num_predictions, attributes]
                        cv::Mat attrsByPred = cv::Mat(dim1, dim2, CV_32F, const_cast<float*>(output.ptr<float>(0)));
                        view = attrsByPred.t();
                    }
                    appendDetectionsFrom2D(view);
                }
            }
        }

        if (boxes.empty()) {
            return false;
        }

        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, 0.25f, 0.45f, indices);

        for (int index : indices) {
            YoloDetection detection;
            detection.classId = classIds[index];
            detection.confidence = confidences[index];
            detection.box = boxes[index];
            detections.push_back(detection);
        }
    } catch (const cv::Exception& ex) {
        qWarning() << "YOLO inference failed; disabling AI for this session:" << ex.what();
        m_aiEnabled = false;
        m_targetClassPending = false;
        return false;
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

void CameraWorker::setYoloModel(const QString& modelPath, const QStringList& classNames) {
    std::lock_guard<std::mutex> lock(m_yoloMutex);
    const bool modelChanged = (m_yoloModelPath != modelPath) || (m_yoloClassNames != classNames);
    m_yoloModelPath = modelPath;
    m_yoloClassNames = classNames;
    if (modelChanged) {
        m_yoloNetDirty = true;
        m_yoloNetLoaded = false;
        m_yoloOutputInfoLogged = false;
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
