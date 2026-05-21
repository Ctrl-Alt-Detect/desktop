#include "video_export_engine.h"
#include "yoloassist.h"
#include <QProgressDialog>
#include <QCoreApplication>
#include <QMessageBox>
#include <QFile>
#include <QDebug>

VideoExportEngine::VideoExportEngine() = default;
VideoExportEngine::~VideoExportEngine() = default;

cv::Ptr<cv::Tracker> VideoExportEngine::createTrackerByName(const QString& trackerName) {
    const QString name = trackerName.trimmed();
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

std::vector<cv::Ptr<cv::Tracker>> VideoExportEngine::buildTrackers(const QStringList& selectedTypes) {
    std::vector<cv::Ptr<cv::Tracker>> trackers;
    for (const QString& type : selectedTypes) {
        try {
            cv::Ptr<cv::Tracker> tracker = createTrackerByName(type);
            if (tracker) {
                trackers.push_back(tracker);
            }
        } catch (const cv::Exception& ex) {
            qWarning() << "Failed to create tracker" << type << ":" << ex.what();
        }
    }
    if (trackers.empty()) {
        trackers.push_back(cv::TrackerCSRT::create());
    }
    return trackers;
}

void VideoExportEngine::reinitializeTrackersAt(const cv::Rect2d& box, const cv::Mat& frame, 
                                               std::vector<cv::Ptr<cv::Tracker>>& trackers) {
    cv::Rect initRect = static_cast<cv::Rect>(box);
    initRect.x = std::max(0, std::min(initRect.x, std::max(0, frame.cols - 1)));
    initRect.y = std::max(0, std::min(initRect.y, std::max(0, frame.rows - 1)));
    initRect.width = std::max(1, std::min(initRect.width, std::max(1, frame.cols - initRect.x)));
    initRect.height = std::max(1, std::min(initRect.height, std::max(1, frame.rows - initRect.y)));

    int initCount = 0;
    for (auto& tracker : trackers) {
        if (!tracker) {
            continue;
        }
        try {
            tracker->init(frame, initRect);
            ++initCount;
        } catch (const cv::Exception& ex) {
            qWarning() << "Export tracker reinit failed:" << ex.what();
        }
    }

    if (initCount == 0) {
        try {
            trackers.clear();
            cv::Ptr<cv::Tracker> fallback = cv::TrackerCSRT::create();
            fallback->init(frame, initRect);
            trackers.push_back(fallback);
        } catch (const cv::Exception& ex) {
            qWarning() << "Export fallback tracker reinit failed:" << ex.what();
        }
    }
}

bool VideoExportEngine::export_video(const ExportSettings& settings, TimelineRepository* timeline, 
                                      QProgressDialog* progressDialog) {
    m_lastError.clear();

    cv::VideoCapture cap(settings.sourcePath.toStdString());
    if (!cap.isOpened()) {
        m_lastError = "Failed to open source video";
        return false;
    }

    int frameWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frameHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    if (frameWidth <= 0 || frameHeight <= 0) {
        m_lastError = "Invalid source video dimensions";
        cap.release();
        return false;
    }

    double fps = cap.get(cv::CAP_PROP_FPS);
    if (!(fps > 1.0 && fps < 240.0)) {
        fps = 30.0;
    }

    const int totalFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    const int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    cv::VideoWriter writer;
    if (!writer.open(settings.outputPath.toStdString(), fourcc, fps, cv::Size(frameWidth, frameHeight), true)) {
        m_lastError = "Failed to create output video file";
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
                    if (!tracker) {
                        continue;
                    }
                    try {
                        tracker->init(frame, initRect);
                        ++initCount;
                    } catch (const cv::Exception& ex) {
                        qWarning() << "Export tracker init failed:" << ex.what();
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
                        qWarning() << "Export fallback tracker init failed:" << ex.what();
                    }
                }
                trackingInitialized = (initCount > 0);
            } else {
                // Multi-tracker averaging
                double sumX = 0.0, sumY = 0.0, sumW = 0.0, sumH = 0.0;
                int successCount = 0;
                for (auto& tracker : trackers) {
                    if (!tracker) {
                        continue;
                    }
                    cv::Rect currentBox = static_cast<cv::Rect>(trackerBox);
                    bool updated = false;
                    try {
                        updated = tracker->update(frame, currentBox);
                    } catch (const cv::Exception& ex) {
                        qWarning() << "Export tracker update failed:" << ex.what();
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
                            if (detection.classId == targetClassId) {
                                matchingDetections.push_back(detection);
                            }
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
                    qWarning() << "Export YOLO inference failed; disabling AI assist:" << yoloError;
                    exportAiAssistEnabled = false;
                    targetClassPending = false;
                }
            }

            // Draw tracking box
            if (trackingInitialized) {
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

        writer.write(frame);
        ++frameIndex;
        if (progressDialog) {
            progressDialog->setValue(frameIndex);
            QCoreApplication::processEvents();
        }
    }

    writer.release();
    cap.release();

    if (canceled) {
        QFile::remove(settings.outputPath);
        return false;
    }

    return true;
}
