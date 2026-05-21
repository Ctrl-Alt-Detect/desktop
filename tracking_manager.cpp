#include "tracking_manager.h"
#include <QDebug>

TrackingManager::TrackingManager()
    : m_trackerTypes({"CSRT"}), m_trackerActive(false), m_trackerInitialized(false) {
    m_trackers.push_back(createTrackerByName("CSRT"));
}

TrackingManager::~TrackingManager() {
    m_trackers.clear();
}

cv::Ptr<cv::Tracker> TrackingManager::createTrackerByName(const QString& trackerType) {
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
    return nullptr;
}

void TrackingManager::setTrackerTypes(const QStringList& trackerTypes) {
    m_trackerTypes = trackerTypes;
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
    m_trackerInitialized = false;
}

bool TrackingManager::initialize(const cv::Mat& frame, const cv::Rect2d& box) {
    if (m_trackers.empty()) {
        setTrackerTypes(m_trackerTypes);
    }

    cv::Rect initRect = static_cast<cv::Rect>(box);
    initRect.x = std::max(0, std::min(initRect.x, std::max(0, frame.cols - 1)));
    initRect.y = std::max(0, std::min(initRect.y, std::max(0, frame.rows - 1)));
    initRect.width = std::max(1, std::min(initRect.width, std::max(1, frame.cols - initRect.x)));
    initRect.height = std::max(1, std::min(initRect.height, std::max(1, frame.rows - initRect.y)));

    m_trackerBox = cv::Rect2d(initRect.x, initRect.y, initRect.width, initRect.height);

    int initCount = 0;
    for (auto& tracker : m_trackers) {
        if (!tracker) continue;
        try {
            tracker->init(frame, initRect);
            ++initCount;
        } catch (const cv::Exception& ex) {
            qWarning() << "Tracker init failed:" << ex.what();
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
            qWarning() << "Tracker fallback init failed:" << ex.what();
        }
    }

    m_trackerInitialized = (initCount > 0);
    return m_trackerInitialized;
}

cv::Rect2d TrackingManager::update(const cv::Mat& frame) {
    // If we have a pending box to initialize, do it now
    if (m_hasPendingBox && m_trackerActive) {
        m_hasPendingBox = false;
        initialize(frame, m_pendingBox);
        return m_trackerBox;
    }

    if (!m_trackerInitialized || m_trackers.empty()) {
        return m_trackerBox;
    }

    double sumX = 0.0, sumY = 0.0, sumW = 0.0, sumH = 0.0;
    int successCount = 0;

    for (auto& tracker : m_trackers) {
        if (!tracker) continue;

        cv::Rect currentBox = static_cast<cv::Rect>(m_trackerBox);
        bool updated = false;
        try {
            updated = tracker->update(frame, currentBox);
        } catch (const cv::Exception& ex) {
            qWarning() << "Tracker update failed:" << ex.what();
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
        m_trackerBox = cv::Rect2d(sumX / successCount, sumY / successCount,
                                   sumW / successCount, sumH / successCount);
    }

    return m_trackerBox;
}

void TrackingManager::reset() {
    m_trackers.clear();
    m_trackerInitialized = false;
    m_trackerActive = false;
    m_trackerBox = cv::Rect2d();
    m_hasPendingBox = false;
    m_pendingBox = cv::Rect2d();
}

void TrackingManager::setPendingBox(const cv::Rect2d& box) {
    m_pendingBox = box;
    m_hasPendingBox = true;
    m_trackerInitialized = false;
}
