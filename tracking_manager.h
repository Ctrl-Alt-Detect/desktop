#pragma once
#include <QString>
#include <QStringList>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>

class TrackingManager {
public:
    TrackingManager();
    ~TrackingManager();

    /// Factory: creates a tracker by name (case-insensitive)
    static cv::Ptr<cv::Tracker> createTrackerByName(const QString& trackerType);

    /// Set active tracker types and rebuild tracker collection
    void setTrackerTypes(const QStringList& trackerTypes);

    /// Initialize trackers with a frame and initial box
    bool initialize(const cv::Mat& frame, const cv::Rect2d& box);

    /// Set a pending box to be initialized on next update (used when box is known but frame is not available yet)
    void setPendingBox(const cv::Rect2d& box);

    /// Update trackers with new frame; returns averaged box from all successful trackers
    cv::Rect2d update(const cv::Mat& frame);

    /// Reset tracker state
    void reset();

    /// Query tracker state
    bool isInitialized() const { return m_trackerInitialized; }
    bool isActive() const { return m_trackerActive; }
    cv::Rect2d box() const { return m_trackerBox; }

    /// Set tracker as active/inactive
    void setActive(bool active) { m_trackerActive = active; }

private:
    QStringList m_trackerTypes;
    std::vector<cv::Ptr<cv::Tracker>> m_trackers;
    bool m_trackerActive{false};
    bool m_trackerInitialized{false};
    cv::Rect2d m_trackerBox;
    cv::Rect2d m_pendingBox;
    bool m_hasPendingBox{false};
};
