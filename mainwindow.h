#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QMap>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QList>
#include <QAction>
#include <QListWidget>
#include <QKeyEvent>
#include <QTextEdit>
#include <QScrollBar>
#include "cameraworker.h"
#include "videolabel.h"
#include "timeline_repository.h"
#include "application_settings.h"
#include "video_export_engine.h"
#include "yolo_export_engine.h"
#include "debug_overlay_renderer.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void updateFrame(const QImage& frame);
    void onDebugInfoReady(const QString& info);
    void onCameraClicked();
    void onOpenVideoClicked();
    void onPlayPauseClicked();
    void onSpeedChanged(int value);
    void onSeekReleased();
    void onSeekValueChanged(int value);
    void onVideoInfo(int totalFrames, double fps);
    void onVideoPosition(int frameIndex);
    void onPlaybackEnded();
    void onTrackerSelection(int x, int y, int width, int height);
    void onTrackerReset();
    void onCameraResolutionChanged(int index);
    void onExportVideoClicked();
    void onExportYoloClicked();
    void onRemoveEventClicked();
    void onClearEventsClicked();
    void onEventActivated(QListWidgetItem* item);
    void onTrackerActionToggled(bool checked);
    void onLoadYoloModelClicked();
    void onAiEnabledChanged(bool checked);
    void onAiIntervalChanged(int index);

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    int currentVideoFrame() const;
    void applyTrackingEventForFrame(int frameIndex);
    void updateSeekTimeLabel(int currentFrame);
    QStringList selectedTrackerTypes() const;
    void syncTrackerButtonText();
    void applyTrackerSelectionToWorker();
    void applyAiSettingsToWorker();
    void refreshTrackingEventsUi();
    void refreshYoloStatusLabel();
    QString trackingEventText(int frameIndex, const TrackingEvent& event) const;
    QString frameToTimeText(int frameIndex) const;
    QStringList loadClassNamesFromFile(const QString& filePath) const;

    void startSource(const QString& source, bool useGstreamer);
    void stopCameraThread();
    void setVideoControlsEnabled(bool enabled);
    QString buildCameraPipeline() const;

    QString m_defaultCameraPipeline{"mfvideosrc ! video/x-raw,width=640,height=480 ! videoconvert ! video/x-raw,format=BGR ! appsink"};
    QThread* m_thread{nullptr};
    VideoLabel* m_videoLabel{nullptr};
    CameraWorker* m_camera{nullptr};
    QPushButton* m_playPauseButton{nullptr};
    QPushButton* m_exportVideoButton{nullptr};
    QPushButton* m_exportYoloButton{nullptr};
    QLabel* m_resolutionTextLabel{nullptr};
    QComboBox* m_resolutionCombo{nullptr};
    QLabel* m_seekTextLabel{nullptr};
    QLabel* m_seekTimeLabel{nullptr};
    QLabel* m_speedTextLabel{nullptr};
    QLabel* m_speedValueLabel{nullptr};
    QLabel* m_trackerTextLabel{nullptr};
    QPushButton* m_trackerPickerButton{nullptr};
    QPushButton* m_loadYoloButton{nullptr};
    QCheckBox* m_aiEnabledCheckBox{nullptr};
    QComboBox* m_aiIntervalCombo{nullptr};
    QLabel* m_yoloStatusLabel{nullptr};
    QLabel* m_eventsTextLabel{nullptr};
    QSlider* m_seekSlider{nullptr};
    QSlider* m_speedSlider{nullptr};
    QListWidget* m_eventsList{nullptr};
    QPushButton* m_removeEventButton{nullptr};
    QPushButton* m_clearEventsButton{nullptr};
    DebugOverlayRenderer* m_debugOverlay{nullptr};
    QList<QAction*> m_trackerActions;
    QString m_yoloModelPath;
    QStringList m_yoloClassNames;
    TimelineRepository* m_timeline{nullptr};
    ApplicationSettings* m_settings{nullptr};
    int m_lastAppliedTimelineFrame{-1};
    int m_totalVideoFrames{0};
    double m_videoFps{30.0};
    QString m_currentVideoPath;
    bool m_isVideoMode{false};
    bool m_isPaused{false};
};