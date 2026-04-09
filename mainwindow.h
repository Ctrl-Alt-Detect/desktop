#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QString>
#include "cameraworker.h"
#include "videolabel.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void updateFrame(const QImage& frame);
    void onCameraClicked();
    void onOpenVideoClicked();
    void onPlayPauseClicked();
    void onSpeedChanged(int value);
    void onSeekReleased();
    void onVideoInfo(int totalFrames, double fps);
    void onVideoPosition(int frameIndex);
    void onPlaybackEnded();
    void onTrackerSelection(int x, int y, int width, int height);
    void onTrackerReset();
    void onCameraResolutionChanged(int index);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void startSource(const QString& source, bool useGstreamer);
    void stopCameraThread();
    void setVideoControlsEnabled(bool enabled);
    QString buildCameraPipeline() const;

    QString m_defaultCameraPipeline{"mfvideosrc ! video/x-raw,width=640,height=480 ! videoconvert ! video/x-raw,format=BGR ! appsink"};
    QThread* m_thread{nullptr};
    VideoLabel* m_videoLabel{nullptr};
    CameraWorker* m_camera{nullptr};
    QPushButton* m_playPauseButton{nullptr};
    QLabel* m_resolutionTextLabel{nullptr};
    QComboBox* m_resolutionCombo{nullptr};
    QLabel* m_seekTextLabel{nullptr};
    QLabel* m_speedTextLabel{nullptr};
    QLabel* m_speedValueLabel{nullptr};
    QSlider* m_seekSlider{nullptr};
    QSlider* m_speedSlider{nullptr};
    bool m_isVideoMode{false};
    bool m_isPaused{false};
};