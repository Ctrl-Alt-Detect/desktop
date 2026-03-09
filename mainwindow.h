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
#include "aicore.h"

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
    void onToggleAI();
    void onSaveAIModel();
    void onLoadAIModel();
    void onAIFrameIntervalChanged(int value);
    void onSetupYOLO();

private:
    void setupUI();
    void populateCameras();
    void populateTrackers();
    cv::Rect convertQRectToCvRect(const QRect &qrect, const QSize &labelSize, const QSize &frameSize);
    cv::Ptr<cv::Tracker> createTracker(TrackerType type);
    QList<TrackerType> getSelectedTrackers();
    void updateDrawingEnabled();
    void updateFpsDisplay();
    void runAIDetection(cv::Mat &displayFrame);
    void reinitTrackers(const cv::Rect &newRect);

    // UI Components
    QGroupBox *m_cameraGroup;
    QComboBox *m_cameraCombo;
    QListWidget *m_trackerList;
    VideoLabel *m_videoLabel;
    OutlineButton *m_stopTrackingBtn;
    OutlineButton *m_clearSelectionBtn;
    OutlineButton *m_aiToggleBtn;
    OutlineButton *m_aiSaveBtn;
    OutlineButton *m_aiLoadBtn;
    QLabel *m_statusLabel;
    QLabel *m_fpsLabel;
    QLabel *m_delayLabel;
    QLabel *m_aiStatusLabel;
    QComboBox *m_aiFrameIntervalCombo;

    // OpenCV Components
    cv::VideoCapture m_capture;
    cv::Mat m_currentFrame;
    QTimer *m_timer;
    
    // Tracking
    QList<cv::Ptr<cv::Tracker>> m_trackers;
    QList<cv::Rect> m_trackingRects;
    cv::Rect m_averagedRect;
    bool m_isTracking;

    // AI Components
    AICore m_aiCore;
    bool m_aiEnabled = false;
    int m_frameCounter = 0;
    int m_aiFrameInterval = 15;

    // Performance metrics
    QElapsedTimer m_frameTimer;
    QList<qint64> m_frameTimes;
    qint64 m_lastProcessingTime;
};

#endif // MAINWINDOW_H
