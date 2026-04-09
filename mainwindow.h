#pragma once
#include <QMainWindow>
#include <QLabel>
#include "cameraworker.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void updateFrame(const QImage& frame);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void stopCameraThread();

    QThread * m_thread;
    QLabel* m_videoLabel;
    CameraWorker* m_camera;
};