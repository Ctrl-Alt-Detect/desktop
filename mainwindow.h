#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QString>
#include "cameraworker.h"

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

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void startSource(const QString& source, bool useGstreamer);
    void stopCameraThread();
    void setVideoControlsEnabled(bool enabled);

    QString m_defaultCameraPipeline{"mfvideosrc ! videoconvert ! video/x-raw,format=BGR ! appsink"};
    QThread* m_thread{nullptr};
    QLabel* m_videoLabel{nullptr};
    CameraWorker* m_camera{nullptr};
    QPushButton* m_playPauseButton{nullptr};
    QSlider* m_seekSlider{nullptr};
    QSlider* m_speedSlider{nullptr};
    bool m_isVideoMode{false};
    bool m_isPaused{false};
};