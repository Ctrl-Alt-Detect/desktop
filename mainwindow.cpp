#include "mainwindow.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Qt5 GStreamer Webcam");
    resize(640, 480);

    auto* central = new QWidget(this);
    setCentralWidget(central);

    m_videoLabel = new QLabel(this);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumSize(640, 480);
    m_videoLabel->setScaledContents(true);
    // m_videoLabel->setStyleSheet("background: black;");

    auto* layout = new QVBoxLayout(central);
    layout->addWidget(m_videoLabel);

    m_camera = new CameraWorker;
    m_thread = new QThread(this); 

    m_camera->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_camera, &CameraWorker::run);
    connect(m_camera, &CameraWorker::frameReady, this, &MainWindow::updateFrame, Qt::QueuedConnection);
    connect(m_camera, &CameraWorker::finished, m_thread, &QThread::quit);
    connect(m_camera, &CameraWorker::finished, m_camera, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    m_thread->start();
}

MainWindow::~MainWindow() {
    stopCameraThread();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    stopCameraThread();
    QMainWindow::closeEvent(event);
}

void MainWindow::stopCameraThread() {
    if (m_camera) {
        m_camera->stop();
    }

    if (m_thread) {
        m_thread->requestInterruption();
        m_thread->quit();
        if (!m_thread->wait(2000)) {
            qWarning() << "Camera thread did not stop in time; forcing termination";
            m_thread->terminate();
            m_thread->wait(1000);
        }
    }

    m_camera = nullptr;
    m_thread = nullptr;
}

void MainWindow::updateFrame(const QImage& frame) {
    m_videoLabel->setPixmap(QPixmap::fromImage(frame));
}
