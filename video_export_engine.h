#ifndef VIDEO_EXPORT_ENGINE_H
#define VIDEO_EXPORT_ENGINE_H

#include <QString>
#include <QStringList>
#include <QProgressDialog>
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include "timeline_repository.h"

/**
 * VideoExportEngine - Handles exporting tracked video to MP4/AVI format
 * 
 * Encapsulates the logic for:
 * - Video capture and frame iteration
 * - Multi-tracker initialization and averaging
 * - YOLO detection-based tracker correction
 * - Video writing with drawn bounding boxes
 * - Progress tracking and cancellation
 */
class VideoExportEngine {
public:
    struct ExportSettings {
        QString sourcePath;
        QString outputPath;
        QStringList trackerTypes;
        bool aiAssistEnabled = false;
        QString yoloModelPath;
        QStringList yoloClassNames;
        int aiIntervalFrames = 30;
    };

    VideoExportEngine();
    ~VideoExportEngine();

    /**
     * Export video with tracking overlays
     * @param settings Export configuration
     * @param timeline Tracking events by frame
     * @param progressDialog Optional progress UI
     * @return true if export succeeded, false if canceled or failed
     */
    bool export_video(const ExportSettings& settings, TimelineRepository* timeline, QProgressDialog* progressDialog = nullptr);

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

#endif // VIDEO_EXPORT_ENGINE_H
