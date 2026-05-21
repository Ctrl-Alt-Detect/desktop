#ifndef YOLO_EXPORT_ENGINE_H
#define YOLO_EXPORT_ENGINE_H

#include <QString>
#include <QStringList>
#include <QProgressDialog>
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include "timeline_repository.h"

/**
 * YoloExportEngine - Handles exporting tracking data as YOLO format dataset
 * 
 * Encapsulates the logic for:
 * - Video frame extraction to images
 * - Multi-tracker initialization and averaging
 * - YOLO detection-based tracker correction 
 * - YOLO label file generation (normalized bbox format)
 * - Progress tracking and cancellation
 */
class YoloExportEngine {
public:
    struct ExportSettings {
        QString sourcePath;
        QString outputDirectory;
        QStringList trackerTypes;
        bool aiAssistEnabled = false;
        QString yoloModelPath;
        QStringList yoloClassNames;
        int aiIntervalFrames = 30;
    };

    YoloExportEngine();
    ~YoloExportEngine();

    /**
     * Export video as YOLO dataset with labels
     * @param settings Export configuration
     * @param timeline Tracking events by frame
     * @param progressDialog Optional progress UI
     * @return true if export succeeded, false if canceled or failed
     */
    bool export_yolo(const ExportSettings& settings, TimelineRepository* timeline, QProgressDialog* progressDialog = nullptr);

    /**
     * Get last error message
     */
    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;

    cv::Ptr<cv::Tracker> createTrackerByName(const QString& trackerName);
    std::vector<cv::Ptr<cv::Tracker>> buildTrackers(const QStringList& selectedTypes);
    void reinitializeTrackersAt(const cv::Rect2d& box, const cv::Mat& frame, std::vector<cv::Ptr<cv::Tracker>>& trackers);
};

#endif // YOLO_EXPORT_ENGINE_H
