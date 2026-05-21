#include "yolo_export_engine.h"
#include "yoloassist.h"
#include <QProgressDialog>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

YoloExportEngine::YoloExportEngine() = default;
YoloExportEngine::~YoloExportEngine() = default;

cv::Ptr<cv::Tracker> YoloExportEngine::createTrackerByName(const QString& trackerName) {
    const QString name = trackerName.trimmed();
    if (name.compare("CSRT", Qt::CaseInsensitive) == 0) return cv::TrackerCSRT::create();
    if (name.compare("KCF", Qt::CaseInsensitive) == 0) return cv::TrackerKCF::create();
    if (name.compare("MIL", Qt::CaseInsensitive) == 0) return cv::TrackerMIL::create();
    if (name.compare("GOTURN", Qt::CaseInsensitive) == 0) return cv::TrackerGOTURN::create();
    if (name.compare("DaSiamRPN", Qt::CaseInsensitive) == 0) return cv::TrackerDaSiamRPN::create();
    if (name.compare("Nano", Qt::CaseInsensitive) == 0) return cv::TrackerNano::create();
    if (name.compare("Vit", Qt::CaseInsensitive) == 0) return cv::TrackerVit::create();
    return {};
}

std::vector<cv::Ptr<cv::Tracker>> YoloExportEngine::buildTrackers(const QStringList& selectedTypes) {
    std::vector<cv::Ptr<cv::Tracker>> trackers;
    for (const QString& type : selectedTypes) {
        try {
            cv::Ptr<cv::Tracker> tracker = createTrackerByName(type);
            if (tracker) trackers.push_back(tracker);
        } catch (const cv::Exception& ex) {
            qWarning() << "Failed to create tracker" << type << ":" << ex.what();
        }
    }
    if (trackers.empty()) trackers.push_back(cv::TrackerCSRT::create());
    return trackers;
}

void YoloExportEngine::reinitializeTrackersAt(const cv::Rect2d& box, const cv::Mat& frame,
                                              std::vector<cv::Ptr<cv::Tracker>>& trackers) {
    cv::Rect initRect = static_cast<cv::Rect>(box);
    initRect.x = std::max(0, std::min(initRect.x, std::max(0, frame.cols - 1)));
    initRect.y = std::max(0, std::min(initRect.y, std::max(0, frame.rows - 1)));
    initRect.width = std::max(1, std::min(initRect.width, std::max(1, frame.cols - initRect.x)));
    initRect.height = std::max(1, std::min(initRect.height, std::max(1, frame.rows - initRect.y)));

    int initCount = 0;
    for (auto& tracker : trackers) {
        if (!tracker) continue;
        try {
            tracker->init(frame, initRect);
            ++initCount;
        } catch (const cv::Exception& ex) {
            qWarning() << "Export (YOLO) tracker reinit failed:" << ex.what();
        }
    }

    if (initCount == 0) {
        try {
            trackers.clear();
            cv::Ptr<cv::Tracker> fallback = cv::TrackerCSRT::create();
            fallback->init(frame, initRect);
            trackers.push_back(fallback);
        } catch (const cv::Exception& ex) {
            qWarning() << "Export (YOLO) fallback tracker reinit failed:" << ex.what();
        }
    }
}

bool YoloExportEngine::export_yolo(const ExportSettings& settings, TimelineRepository* timeline,
                                   QProgressDialog* progressDialog) {
    m_lastError.clear();

    cv::VideoCapture cap(settings.sourcePath.toStdString());
    if (!cap.isOpened()) {
        m_lastError = "Failed to open source video";
        return false;
    }

    const int frameWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    const int frameHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    if (frameWidth <= 0 || frameHeight <= 0) {
        m_lastError = "Invalid source video dimensions";
        cap.release();
        return false;
    }

    const int totalFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    
    // Create output directories
    const QFileInfo inputInfo(settings.sourcePath);
    const QString baseDir = settings.outputDirectory + "/" + inputInfo.completeBaseName() + "_yolo";
    const QString imagesDir = baseDir + "/images";
    const QString labelsDir = baseDir + "/labels";
    QDir dir;
    if (!dir.mkpath(imagesDir)) {
        m_lastError = "Failed to create images directory: " + imagesDir;
        cap.release();
        return false;
    }
    if (!dir.mkpath(labelsDir)) {
        m_lastError = "Failed to create labels directory: " + labelsDir;
        cap.release();
        return false;
    }

    std::vector<cv::Ptr<cv::Tracker>> trackers;
    bool trackingActive = false;
    bool trackingInitialized = false;
    cv::Rect2d trackerBox;
    
    bool exportAiAssistEnabled = settings.aiAssistEnabled && !settings.yoloModelPath.isEmpty();
    int targetClassId = -1;
    bool targetClassPending = false;
    YoloAssist exportYoloAssist;
    if (exportAiAssistEnabled) {
        exportYoloAssist.setModel(settings.yoloModelPath, settings.yoloClassNames);
    }

    cv::Mat frame;
    int frameIndex = 0;
    bool canceled = false;

    while (cap.read(frame)) {
        if (progressDialog && progressDialog->wasCanceled()) {
            canceled = true;
            break;
        }

        // Check timeline for SetRoi or Stop events
        const TrackingEvent* evt = timeline->event(frameIndex);
        if (evt != nullptr) {
            const TrackingEvent& event = *evt;
            if (event.type == TrackingEventType::SetRoi) {
                trackerBox = cv::Rect2d(event.roi.x(), event.roi.y(), event.roi.width(), event.roi.height());
                trackers = buildTrackers(settings.trackerTypes);
                trackingActive = true;
                trackingInitialized = false;
                if (exportAiAssistEnabled) {
                    targetClassId = -1;
                    targetClassPending = true;
                }
            } else {
                trackers.clear();
                trackingActive = false;
                trackingInitialized = false;
                targetClassId = -1;
                targetClassPending = false;
            }
        }

        if (trackingActive && !trackers.empty()) {
            if (!trackingInitialized) {
                const cv::Rect initRect = static_cast<cv::Rect>(trackerBox);
                int initCount = 0;
                for (auto& tracker : trackers) {
                    if (!tracker) continue;
                    try {
                        tracker->init(frame, initRect);
                        ++initCount;
                    } catch (const cv::Exception& ex) {
                        qWarning() << "Export (YOLO) tracker init failed:" << ex.what();
                    }
                }
                if (initCount == 0) {
                    try {
                        trackers.clear();
                        cv::Ptr<cv::Tracker> fallback = cv::TrackerCSRT::create();
                        fallback->init(frame, initRect);
                        trackers.push_back(fallback);
                        initCount = 1;
                    } catch (const cv::Exception& ex) {
                        qWarning() << "Export (YOLO) fallback tracker init failed:" << ex.what();
                    }
                }
                trackingInitialized = (initCount > 0);
            } else {
                // Multi-tracker averaging
                double sumX = 0.0, sumY = 0.0, sumW = 0.0, sumH = 0.0;
                int successCount = 0;
                for (auto& tracker : trackers) {
                    if (!tracker) continue;
                    cv::Rect currentBox = static_cast<cv::Rect>(trackerBox);
                    bool updated = false;
                    try {
                        updated = tracker->update(frame, currentBox);
                    } catch (const cv::Exception& ex) {
                        qWarning() << "Export (YOLO) tracker update failed:" << ex.what();
                    }
                    if (updated && currentBox.width > 0 && currentBox.height > 0) {
                        sumX += currentBox.x;
                        sumY += currentBox.y;
                        sumW += currentBox.width;
                        sumH += currentBox.height;
                        ++successCount;
                    }
                }
                if (successCount > 0) {
                    trackerBox = cv::Rect2d(sumX / successCount, sumY / successCount, sumW / successCount, sumH / successCount);
                }
            }

            // YOLO AI correction
            const bool runAiPass = exportAiAssistEnabled && 
                                  (targetClassPending || (settings.aiIntervalFrames > 0 && frameIndex % settings.aiIntervalFrames == 0));
            if (runAiPass) {
                std::vector<YoloAssist::Detection> detections;
                QString yoloError;
                if (exportYoloAssist.detect(frame, detections, &yoloError) && !detections.empty()) {
                    YoloAssist::Detection selectedDetection;
                    bool foundDetection = false;
                    if (targetClassPending) {
                        foundDetection = YoloAssist::chooseDetectionForSelection(detections, trackerBox, selectedDetection);
                    }
                    if (!foundDetection && targetClassId >= 0) {
                        std::vector<YoloAssist::Detection> matchingDetections;
                        for (const YoloAssist::Detection& detection : detections) {
                            if (detection.classId == targetClassId) matchingDetections.push_back(detection);
                        }
                        if (!matchingDetections.empty()) {
                            foundDetection = YoloAssist::chooseDetectionForCorrection(matchingDetections, trackerBox, selectedDetection);
                        }
                    }
                    if (!foundDetection) {
                        foundDetection = YoloAssist::chooseDetectionForCorrection(detections, trackerBox, selectedDetection);
                    }
                    if (foundDetection) {
                        reinitializeTrackersAt(
                            cv::Rect2d(selectedDetection.box.x, selectedDetection.box.y, selectedDetection.box.width, selectedDetection.box.height),
                            frame, trackers);
                        if (targetClassPending) {
                            targetClassId = selectedDetection.classId;
                            targetClassPending = false;
                        }
                    }
                } else if (!yoloError.isEmpty()) {
                    qWarning() << "Export (YOLO) inference failed; disabling AI assist:" << yoloError;
                    exportAiAssistEnabled = false;
                    targetClassPending = false;
                }
            }
        }

        // Save frame as image
        const QString imageFile = QString("%1/frame_%2.png").arg(imagesDir).arg(frameIndex, 6, 10, QChar('0'));
        try {
            cv::imwrite(imageFile.toStdString(), frame);
        } catch (const cv::Exception& ex) {
            qWarning() << "Failed to save frame image:" << ex.what();
        }

        // Save YOLO label file
        const QString labelFile = QString("%1/frame_%2.txt").arg(labelsDir).arg(frameIndex, 6, 10, QChar('0'));
        if (trackingInitialized) {
            const double x = trackerBox.x;
            const double y = trackerBox.y;
            const double w = trackerBox.width;
            const double h = trackerBox.height;
            const double xCenter = (x + w / 2.0) / static_cast<double>(frameWidth);
            const double yCenter = (y + h / 2.0) / static_cast<double>(frameHeight);
            const double wN = w / static_cast<double>(frameWidth);
            const double hN = h / static_cast<double>(frameHeight);
            const int classIdToWrite = (targetClassId >= 0) ? targetClassId : 0;
            
            QFile file(labelFile);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out.setRealNumberPrecision(6);
                out << classIdToWrite << " " << xCenter << " " << yCenter << " " << wN << " " << hN << "\n";
                file.close();
            }
        }

        ++frameIndex;
        if (progressDialog) {
            progressDialog->setValue(frameIndex);
            QCoreApplication::processEvents();
        }
    }

    cap.release();

    if (canceled) {
        return false;
    }

    return true;
}
