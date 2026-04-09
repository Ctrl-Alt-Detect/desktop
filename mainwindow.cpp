#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QDebug>
#include <QCloseEvent>
#include <QSizePolicy>
#include <QComboBox>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Qt5 OpenCV Viewer");
    resize(640, 480);

    auto* central = new QWidget(this);
    setCentralWidget(central);

    m_videoLabel = new VideoLabel(this);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumSize(320, 240);
    m_videoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_videoLabel->setScaledContents(true);
    // m_videoLabel->setStyleSheet("background: black;");

    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    auto* controls = new QHBoxLayout;
    auto* cameraButton = new QPushButton("Camera", this);
    auto* openVideoButton = new QPushButton("Open Video", this);
    m_playPauseButton = new QPushButton("Pause", this);
    m_resolutionTextLabel = new QLabel("Camera", this);
    m_resolutionCombo = new QComboBox(this);
    m_seekTextLabel = new QLabel("Seek", this);
    m_speedTextLabel = new QLabel("Speed", this);
    m_speedValueLabel = new QLabel("1.00x", this);
    m_seekSlider = new QSlider(Qt::Horizontal, this);
    m_speedSlider = new QSlider(Qt::Horizontal, this);

    m_seekSlider->setRange(0, 0);
    m_seekSlider->setMinimumWidth(220);
    m_speedSlider->setRange(25, 300);
    m_speedSlider->setValue(100);
    m_speedSlider->setFixedWidth(140);
    m_resolutionCombo->addItem("640 x 480", QSize(640, 480));
    m_resolutionCombo->addItem("1280 x 720", QSize(1280, 720));
    m_resolutionCombo->addItem("1920 x 1080", QSize(1920, 1080));
    m_resolutionCombo->setCurrentIndex(0);
    m_resolutionCombo->setFixedWidth(120);

    controls->addWidget(cameraButton);
    controls->addWidget(openVideoButton);
    controls->addWidget(m_playPauseButton);
    controls->addWidget(m_resolutionTextLabel);
    controls->addWidget(m_resolutionCombo);
    controls->addWidget(m_seekTextLabel);
    controls->addWidget(m_seekSlider);
    controls->addWidget(m_speedTextLabel);
    controls->addWidget(m_speedSlider);
    controls->addWidget(m_speedValueLabel);
    controls->addStretch();

    layout->addLayout(controls, 0);
    layout->addWidget(m_videoLabel, 1);

    connect(cameraButton, &QPushButton::clicked, this, &MainWindow::onCameraClicked);
    connect(openVideoButton, &QPushButton::clicked, this, &MainWindow::onOpenVideoClicked);
    connect(m_playPauseButton, &QPushButton::clicked, this, &MainWindow::onPlayPauseClicked);
    connect(m_resolutionCombo, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::onCameraResolutionChanged);
    connect(m_speedSlider, &QSlider::valueChanged, this, &MainWindow::onSpeedChanged);
    connect(m_seekSlider, &QSlider::sliderReleased, this, &MainWindow::onSeekReleased);

    setVideoControlsEnabled(false);

    startSource(buildCameraPipeline(), true);
}

MainWindow::~MainWindow() {
    stopCameraThread();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    stopCameraThread();
    QMainWindow::closeEvent(event);
}

void MainWindow::onCameraClicked() {
    startSource(buildCameraPipeline(), true);
}

void MainWindow::onCameraResolutionChanged(int index) {
    Q_UNUSED(index);
    if (!m_isVideoMode) {
        startSource(buildCameraPipeline(), true);
    }
}

void MainWindow::onOpenVideoClicked() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Open Video",
        QString(),
        "Video Files (*.mp4 *.avi *.mkv *.mov *.wmv);;All Files (*.*)");

    if (path.isEmpty()) {
        return;
    }

    startSource(path, false);
}

void MainWindow::onPlayPauseClicked() {
    if (!m_camera || !m_isVideoMode) {
        return;
    }

    m_isPaused = !m_isPaused;
    m_playPauseButton->setText(m_isPaused ? "Play" : "Pause");
    m_camera->setPaused(m_isPaused);
}

void MainWindow::onSpeedChanged(int value) {
    const double speed = static_cast<double>(value) / 100.0;
    if (m_speedValueLabel) {
        m_speedValueLabel->setText(QString::number(speed, 'f', 2) + "x");
    }

    if (!m_camera || !m_isVideoMode) {
        return;
    }

    m_camera->setPlaybackSpeed(speed);
}

void MainWindow::onSeekReleased() {
    if (!m_camera || !m_isVideoMode) {
        return;
    }

    const int frameIndex = m_seekSlider->value();
    m_camera->seekToFrame(frameIndex);
}

void MainWindow::onVideoInfo(int totalFrames, double fps) {
    Q_UNUSED(fps);
    m_seekSlider->setRange(0, totalFrames > 0 ? totalFrames - 1 : 0);
}

void MainWindow::onVideoPosition(int frameIndex) {
    if (!m_seekSlider->isSliderDown()) {
        m_seekSlider->setValue(frameIndex);
    }
}

void MainWindow::onTrackerSelection(int x, int y, int width, int height) {
    if (m_camera) {
        m_camera->initTracker(x, y, width, height);
    }
}

void MainWindow::onTrackerReset() {
    if (m_camera) {
        m_camera->resetTracker();
    }
}

void MainWindow::startSource(const QString& source, bool useGstreamer) {
    stopCameraThread();

    m_isVideoMode = !useGstreamer;
    m_isPaused = false;
    setVideoControlsEnabled(m_isVideoMode);
    if (m_resolutionCombo) {
        m_resolutionCombo->setEnabled(useGstreamer);
    }
    if (m_resolutionTextLabel) {
        m_resolutionTextLabel->setEnabled(useGstreamer);
    }
    m_playPauseButton->setText("Pause");

    m_camera = new CameraWorker;
    m_thread = new QThread(this);

    if (useGstreamer) {
        m_camera->setCameraPipeline(source);
    } else {
        m_camera->setVideoFile(source);
    }

    m_camera->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_camera, &CameraWorker::run);
    connect(m_camera, &CameraWorker::frameReady, this, &MainWindow::updateFrame, Qt::QueuedConnection);
    connect(m_camera, &CameraWorker::finished, m_thread, &QThread::quit);
    connect(m_camera, &CameraWorker::videoInfo, this, &MainWindow::onVideoInfo, Qt::QueuedConnection);
    connect(m_camera, &CameraWorker::positionChanged, this, &MainWindow::onVideoPosition, Qt::QueuedConnection);
    connect(m_videoLabel, &VideoLabel::selectionMade, this, &MainWindow::onTrackerSelection);
    connect(m_videoLabel, &VideoLabel::trackerReset, this, &MainWindow::onTrackerReset);

    m_thread->start();

    if (m_isVideoMode) {
        onSpeedChanged(m_speedSlider->value());
    }
}

QString MainWindow::buildCameraPipeline() const {
    if (!m_resolutionCombo) {
        return m_defaultCameraPipeline;
    }

    const QSize selectedSize = m_resolutionCombo->currentData().toSize();
    if (!selectedSize.isValid()) {
        return m_defaultCameraPipeline;
    }

    return QString("mfvideosrc ! video/x-raw,width=%1,height=%2 ! videoconvert ! video/x-raw,format=BGR ! appsink")
        .arg(selectedSize.width())
        .arg(selectedSize.height());
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

        delete m_thread;
    }

    if (m_camera) {
        delete m_camera;
    }

    m_camera = nullptr;
    m_thread = nullptr;
}

void MainWindow::setVideoControlsEnabled(bool enabled) {
    if (m_playPauseButton) {
        m_playPauseButton->setEnabled(enabled);
    }
    if (m_seekSlider) {
        m_seekSlider->setEnabled(enabled);
        if (!enabled) {
            m_seekSlider->setRange(0, 0);
            m_seekSlider->setValue(0);
        }
    }
    if (m_speedSlider) {
        m_speedSlider->setEnabled(enabled);
    }
    if (m_seekTextLabel) {
        m_seekTextLabel->setEnabled(enabled);
    }
    if (m_speedTextLabel) {
        m_speedTextLabel->setEnabled(enabled);
    }
    if (m_speedValueLabel) {
        m_speedValueLabel->setEnabled(enabled);
    }
}

void MainWindow::updateFrame(const QImage& frame) {
    m_videoLabel->setPixmap(QPixmap::fromImage(frame));
    m_videoLabel->setFrameSize(frame.width(), frame.height());
}
