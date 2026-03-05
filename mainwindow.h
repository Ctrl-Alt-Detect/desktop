#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGroupBox>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTimer>
#include <QLabel>
#include <QListWidget>
#include <QElapsedTimer>
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include "videolabel.h"
#include "outlinebutton.h"

enum TrackerType {
    TRACKER_MIL,
    TRACKER_KCF,
    TRACKER_CSRT
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onCameraChanged(int index);
    void onUpdateFrame();
    void onRectangleSelected(QRect rect);
    void onStopTracking();
    void onClearSelection();
    void onTrackerSelectionChanged();

private:
    void setupUI();
    void populateCameras();
    void populateTrackers();
    cv::Rect convertQRectToCvRect(const QRect &qrect, const QSize &labelSize, const QSize &frameSize);
    cv::Ptr<cv::Tracker> createTracker(TrackerType type);
    QList<TrackerType> getSelectedTrackers();
    void updateDrawingEnabled();
    void updateFpsDisplay();

    // UI Components
    QGroupBox *m_cameraGroup;
    QComboBox *m_cameraCombo;
    QListWidget *m_trackerList;
    VideoLabel *m_videoLabel;
    OutlineButton *m_stopTrackingBtn;
    OutlineButton *m_clearSelectionBtn;
    QLabel *m_statusLabel;
    QLabel *m_fpsLabel;
    QLabel *m_delayLabel;

    // OpenCV Components
    cv::VideoCapture m_capture;
    cv::Mat m_currentFrame;
    QTimer *m_timer;
    
    // Tracking
    QList<cv::Ptr<cv::Tracker>> m_trackers;
    QList<cv::Rect> m_trackingRects;
    cv::Rect m_averagedRect;
    bool m_isTracking;
    
    // Performance metrics
    QElapsedTimer m_frameTimer;
    QList<qint64> m_frameTimes;
    qint64 m_lastProcessingTime;
};

#endif // MAINWINDOW_H
