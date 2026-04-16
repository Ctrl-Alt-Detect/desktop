#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QCheckBox>
#include <QDebug>
#include <QCloseEvent>
#include <QSizePolicy>
#include <QComboBox>
#include <QListWidgetItem>
#include <QMenu>
#include <QAction>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QCoreApplication>
#include <QFile>

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
    m_exportVideoButton = new QPushButton("Export Video", this);
    m_playPauseButton = new QPushButton("Pause", this);
    m_resolutionTextLabel = new QLabel("Camera", this);
    m_resolutionCombo = new QComboBox(this);
    m_seekTextLabel = new QLabel("Seek", this);
    m_seekTimeLabel = new QLabel("00:00.000 / 00:00.000", this);
    m_speedTextLabel = new QLabel("Speed", this);
    m_speedValueLabel = new QLabel("1.00x", this);
    m_trackerTextLabel = new QLabel("Trackers", this);
    m_trackerPickerButton = new QPushButton(this);
    m_loadYoloButton = new QPushButton("Load YOLO", this);
    m_aiEnabledCheckBox = new QCheckBox("AI", this);
    m_aiIntervalCombo = new QComboBox(this);
    m_yoloStatusLabel = new QLabel("YOLO: not loaded", this);
    m_eventsTextLabel = new QLabel("Events", this);
    m_seekSlider = new QSlider(Qt::Horizontal, this);
    m_speedSlider = new QSlider(Qt::Horizontal, this);
    m_eventsList = new QListWidget(this);
    m_removeEventButton = new QPushButton("Remove", this);
    m_clearEventsButton = new QPushButton("Clear", this);

    m_seekSlider->setRange(0, 0);
    m_seekSlider->setMinimumWidth(220);
    m_seekTimeLabel->setMinimumWidth(140);
    m_speedSlider->setRange(25, 300);
    m_speedSlider->setValue(100);
    m_speedSlider->setFixedWidth(140);
    m_resolutionCombo->addItem("640 x 480", QSize(640, 480));
    m_resolutionCombo->addItem("1280 x 720", QSize(1280, 720));
    m_resolutionCombo->addItem("1920 x 1080", QSize(1920, 1080));
    m_resolutionCombo->setCurrentIndex(0);
    m_resolutionCombo->setFixedWidth(120);
    m_trackerPickerButton->setMinimumWidth(120);
    m_loadYoloButton->setFixedWidth(100);
    m_aiEnabledCheckBox->setChecked(false);
    m_aiIntervalCombo->addItem("5 frames", 5);
    m_aiIntervalCombo->addItem("10 frames", 10);
    m_aiIntervalCombo->addItem("20 frames", 20);
    m_aiIntervalCombo->addItem("30 frames", 30);
    m_aiIntervalCombo->addItem("60 frames", 60);
    m_aiIntervalCombo->setCurrentIndex(3);
    m_aiIntervalCombo->setFixedWidth(110);
    m_yoloStatusLabel->setMinimumWidth(180);
    m_eventsList->setMinimumWidth(220);
    m_eventsList->setMaximumWidth(260);
    m_removeEventButton->setFixedWidth(80);
    m_clearEventsButton->setFixedWidth(80);

    controls->addWidget(cameraButton);
    controls->addWidget(openVideoButton);
    controls->addWidget(m_exportVideoButton);
    controls->addWidget(m_playPauseButton);
    controls->addWidget(m_resolutionTextLabel);
    controls->addWidget(m_resolutionCombo);
    controls->addWidget(m_seekTextLabel);
    controls->addWidget(m_seekSlider);
    controls->addWidget(m_seekTimeLabel);
    controls->addWidget(m_speedTextLabel);
    controls->addWidget(m_speedSlider);
    controls->addWidget(m_speedValueLabel);
    controls->addWidget(m_trackerTextLabel);
    controls->addWidget(m_trackerPickerButton);
    controls->addWidget(m_loadYoloButton);
    controls->addWidget(m_aiEnabledCheckBox);
    controls->addWidget(m_aiIntervalCombo);
    controls->addWidget(m_yoloStatusLabel);
    controls->addStretch();

    auto* trackerMenu = new QMenu(m_trackerPickerButton);
    const QStringList availableTrackers{
        "CSRT", "KCF", "MIL", "GOTURN", "DaSiamRPN", "Nano", "Vit"
    };
    for (const QString& trackerName : availableTrackers) {
        QAction* action = trackerMenu->addAction(trackerName);
        action->setCheckable(true);
        action->setChecked(trackerName == "CSRT");
        connect(action, &QAction::toggled, this, &MainWindow::onTrackerActionToggled);
        m_trackerActions.push_back(action);
    }
    m_trackerPickerButton->setMenu(trackerMenu);
    syncTrackerButtonText();

    layout->addLayout(controls, 0);

    auto* contentLayout = new QHBoxLayout;
    contentLayout->setSpacing(8);

    auto* eventsPanel = new QVBoxLayout;
    eventsPanel->addWidget(m_eventsTextLabel, 0);
    eventsPanel->addWidget(m_eventsList, 1);

    auto* eventsButtons = new QHBoxLayout;
    eventsButtons->addWidget(m_removeEventButton);
    eventsButtons->addWidget(m_clearEventsButton);
    eventsButtons->addStretch();
    eventsPanel->addLayout(eventsButtons, 0);

    contentLayout->addLayout(eventsPanel, 0);
    contentLayout->addWidget(m_videoLabel, 1);
    layout->addLayout(contentLayout, 1);

    connect(cameraButton, &QPushButton::clicked, this, &MainWindow::onCameraClicked);
    connect(openVideoButton, &QPushButton::clicked, this, &MainWindow::onOpenVideoClicked);
    connect(m_exportVideoButton, &QPushButton::clicked, this, &MainWindow::onExportVideoClicked);
    connect(m_playPauseButton, &QPushButton::clicked, this, &MainWindow::onPlayPauseClicked);
    connect(m_resolutionCombo, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::onCameraResolutionChanged);
    connect(m_speedSlider, &QSlider::valueChanged, this, &MainWindow::onSpeedChanged);
    connect(m_seekSlider, &QSlider::sliderReleased, this, &MainWindow::onSeekReleased);
    connect(m_seekSlider, &QSlider::valueChanged, this, &MainWindow::onSeekValueChanged);
    connect(m_removeEventButton, &QPushButton::clicked, this, &MainWindow::onRemoveEventClicked);
    connect(m_clearEventsButton, &QPushButton::clicked, this, &MainWindow::onClearEventsClicked);
    connect(m_eventsList, &QListWidget::itemDoubleClicked, this, &MainWindow::onEventActivated);
    connect(m_loadYoloButton, &QPushButton::clicked, this, &MainWindow::onLoadYoloModelClicked);
    connect(m_aiEnabledCheckBox, &QCheckBox::toggled, this, &MainWindow::onAiEnabledChanged);
    connect(m_aiIntervalCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onAiIntervalChanged);

    setVideoControlsEnabled(false);
    refreshYoloStatusLabel();
    applyAiSettingsToWorker();

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

void MainWindow::onExportVideoClicked() {
    if (!m_isVideoMode || m_currentVideoPath.isEmpty()) {
        QMessageBox::information(this, "Export Video", "Open a video file first to export tracking.");
        return;
    }

    const QFileInfo inputInfo(m_currentVideoPath);
    const QString defaultPath = inputInfo.absolutePath() + "/" + inputInfo.completeBaseName() + "_tracked.mp4";
    const QString outputPath = QFileDialog::getSaveFileName(
        this,
        "Export Video With Trackers",
        defaultPath,
        "MP4 Video (*.mp4);;AVI Video (*.avi)");

    if (outputPath.isEmpty()) {
        return;
    }

    cv::VideoCapture cap(m_currentVideoPath.toStdString());
    if (!cap.isOpened()) {
        QMessageBox::warning(this, "Export Video", "Failed to open the source video for export.");
        return;
    }

    int frameWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frameHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    if (frameWidth <= 0 || frameHeight <= 0) {
        QMessageBox::warning(this, "Export Video", "Invalid source video dimensions.");
        return;
    }

    double fps = cap.get(cv::CAP_PROP_FPS);
    if (!(fps > 1.0 && fps < 240.0)) {
        fps = 30.0;
    }

    const int totalFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    const int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    cv::VideoWriter writer;
    if (!writer.open(outputPath.toStdString(), fourcc, fps, cv::Size(frameWidth, frameHeight), true)) {
        QMessageBox::warning(this, "Export Video", "Failed to create output video file.");
        return;
    }

    auto createTrackerByName = [](const QString& trackerName) -> cv::Ptr<cv::Tracker> {
        const QString name = trackerName.trimmed();
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
        return {};
    };

    auto buildTrackers = [&](const QStringList& selectedTypes) {
        std::vector<cv::Ptr<cv::Tracker>> trackers;
        for (const QString& type : selectedTypes) {
            try {
                cv::Ptr<cv::Tracker> tracker = createTrackerByName(type);
                if (tracker) {
                    trackers.push_back(tracker);
                }
            } catch (const cv::Exception& ex) {
                qWarning() << "Failed to create tracker" << type << ":" << ex.what();
            }
        }
        if (trackers.empty()) {
            trackers.push_back(cv::TrackerCSRT::create());
        }
        return trackers;
    };

    QProgressDialog progress("Exporting tracked video...", "Cancel", 0, std::max(0, totalFrames), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    const QStringList selectedTypes = selectedTrackerTypes();
    std::vector<cv::Ptr<cv::Tracker>> trackers;
    bool trackingActive = false;
    bool trackingInitialized = false;
    cv::Rect2d trackerBox;

    cv::Mat frame;
    int frameIndex = 0;
    bool canceled = false;
    while (cap.read(frame)) {
        if (progress.wasCanceled()) {
            canceled = true;
            break;
        }

        const auto eventIt = m_trackingTimeline.constFind(frameIndex);
        if (eventIt != m_trackingTimeline.constEnd()) {
            const TrackingEvent& event = eventIt.value();
            if (event.type == TrackingEventType::SetRoi) {
                trackerBox = cv::Rect2d(event.roi.x(), event.roi.y(), event.roi.width(), event.roi.height());
                trackers = buildTrackers(selectedTypes);
                trackingActive = true;
                trackingInitialized = false;
            } else {
                trackers.clear();
                trackingActive = false;
                trackingInitialized = false;
            }
        }

        if (trackingActive && !trackers.empty()) {
            if (!trackingInitialized) {
                const cv::Rect initRect = static_cast<cv::Rect>(trackerBox);
                int initCount = 0;
                for (auto& tracker : trackers) {
                    if (!tracker) {
                        continue;
                    }
                    try {
                        tracker->init(frame, initRect);
                        ++initCount;
                    } catch (const cv::Exception& ex) {
                        qWarning() << "Export tracker init failed:" << ex.what();
                    }
                }
                if (initCount == 0) {
                    try {
                        trackers.clear();
                        cv::Ptr<cv::Tracker> fallback = cv::TrackerCSRT::create();
                        fallback->init(frame, initRect);
                        trackers.push_back(fallback);
                        initCount = 1;
                    } catch (const cv::Exception& ex) {
                        qWarning() << "Export fallback tracker init failed:" << ex.what();
                    }
                }
                trackingInitialized = (initCount > 0);
            } else {
                double sumX = 0.0;
                double sumY = 0.0;
                double sumW = 0.0;
                double sumH = 0.0;
                int successCount = 0;

                for (auto& tracker : trackers) {
                    if (!tracker) {
                        continue;
                    }
                    cv::Rect currentBox = static_cast<cv::Rect>(trackerBox);
                    bool updated = false;
                    try {
                        updated = tracker->update(frame, currentBox);
                    } catch (const cv::Exception& ex) {
                        qWarning() << "Export tracker update failed:" << ex.what();
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
                    trackerBox = cv::Rect2d(sumX / successCount, sumY / successCount, sumW / successCount, sumH / successCount);
                }
            }

            if (trackingInitialized) {
                cv::Rect drawRect = static_cast<cv::Rect>(trackerBox);
                drawRect.x = std::max(0, drawRect.x);
                drawRect.y = std::max(0, drawRect.y);
                drawRect.width = std::min(drawRect.width, frame.cols - drawRect.x);
                drawRect.height = std::min(drawRect.height, frame.rows - drawRect.y);
                if (drawRect.width > 0 && drawRect.height > 0) {
                    cv::rectangle(frame, drawRect, cv::Scalar(0, 255, 0), 2);
                }
            }
        }

        writer.write(frame);
        ++frameIndex;
        progress.setValue(frameIndex);
        QCoreApplication::processEvents();
    }

    writer.release();
    cap.release();

    if (canceled) {
        QFile::remove(outputPath);
        QMessageBox::information(this, "Export Video", "Export canceled.");
        return;
    }

    progress.setValue(std::max(totalFrames, frameIndex));
    QMessageBox::information(this, "Export Video", "Export complete:\n" + outputPath);
}

void MainWindow::onLoadYoloModelClicked() {
    const QString configPath = QFileDialog::getOpenFileName(
        this,
        "Open YOLO Config",
        QString(),
        "YOLO Config (*.cfg);;All Files (*.*)");
    if (configPath.isEmpty()) {
        return;
    }

    const QString weightsPath = QFileDialog::getOpenFileName(
        this,
        "Open YOLO Weights",
        QFileInfo(configPath).absolutePath(),
        "YOLO Weights (*.weights);;All Files (*.*)");
    if (weightsPath.isEmpty()) {
        return;
    }

    const QString namesPath = QFileDialog::getOpenFileName(
        this,
        "Open YOLO Class Names (optional)",
        QFileInfo(configPath).absolutePath(),
        "Class Names (*.names *.txt);;All Files (*.*)");

    m_yoloConfigPath = configPath;
    m_yoloWeightsPath = weightsPath;
    m_yoloNamesPath = namesPath;
    m_yoloClassNames = namesPath.isEmpty() ? QStringList() : loadClassNamesFromFile(namesPath);

    applyAiSettingsToWorker();
    refreshYoloStatusLabel();
}

void MainWindow::onAiEnabledChanged(bool checked) {
    Q_UNUSED(checked);
    applyAiSettingsToWorker();
    refreshYoloStatusLabel();
}

void MainWindow::onAiIntervalChanged(int index) {
    Q_UNUSED(index);
    applyAiSettingsToWorker();
}

void MainWindow::onPlayPauseClicked() {
    if (!m_camera || !m_isVideoMode) {
        return;
    }

    const bool atVideoEnd = m_seekSlider && (m_seekSlider->maximum() > 0) && (m_seekSlider->value() >= m_seekSlider->maximum());
    if (m_isPaused && atVideoEnd) {
        m_camera->seekToFrame(0);
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
    m_camera->resetTracker();
    m_camera->seekToFrame(frameIndex);
    m_lastAppliedTimelineFrame = -1;
    applyTrackingEventForFrame(frameIndex);
}

void MainWindow::onSeekValueChanged(int value) {
    updateSeekTimeLabel(value);
}

void MainWindow::onVideoInfo(int totalFrames, double fps) {
    if (fps > 1.0 && fps < 240.0) {
        m_videoFps = fps;
    } else {
        m_videoFps = 30.0;
    }
    m_totalVideoFrames = std::max(0, totalFrames);
    m_seekSlider->setRange(0, totalFrames > 0 ? totalFrames - 1 : 0);
    updateSeekTimeLabel(m_seekSlider->value());
}

void MainWindow::onVideoPosition(int frameIndex) {
    if (!m_seekSlider->isSliderDown()) {
        m_seekSlider->setValue(frameIndex);
    }

    applyTrackingEventForFrame(frameIndex);
}

void MainWindow::onPlaybackEnded() {
    if (!m_isVideoMode) {
        return;
    }

    m_isPaused = true;
    if (m_playPauseButton) {
        m_playPauseButton->setText("Play");
    }
}

void MainWindow::onTrackerSelection(int x, int y, int width, int height) {
    if (m_isVideoMode) {
        const int frameIndex = currentVideoFrame();
        if (frameIndex >= 0) {
            TrackingEvent event;
            event.type = TrackingEventType::SetRoi;
            event.roi = QRect(x, y, width, height);
            m_trackingTimeline.insert(frameIndex, event);
            m_lastAppliedTimelineFrame = frameIndex;
            refreshTrackingEventsUi();
        }
    }

    if (m_camera) {
        m_camera->initTracker(x, y, width, height);
    }
}

void MainWindow::onTrackerReset() {
    if (m_isVideoMode) {
        const int frameIndex = currentVideoFrame();
        if (frameIndex >= 0) {
            TrackingEvent event;
            event.type = TrackingEventType::Stop;
            m_trackingTimeline.insert(frameIndex, event);
            m_lastAppliedTimelineFrame = frameIndex;
            refreshTrackingEventsUi();
        }
    }

    if (m_camera) {
        m_camera->resetTracker();
    }
}

void MainWindow::startSource(const QString& source, bool useGstreamer) {
    stopCameraThread();

    m_trackingTimeline.clear();
    m_lastAppliedTimelineFrame = -1;
    m_totalVideoFrames = 0;
    m_videoFps = 30.0;
    refreshTrackingEventsUi();
    updateSeekTimeLabel(0);
    m_isVideoMode = !useGstreamer;
    m_currentVideoPath = useGstreamer ? QString() : source;
    m_isPaused = false;
    setVideoControlsEnabled(m_isVideoMode);
    if (m_trackerTextLabel) {
        m_trackerTextLabel->setEnabled(true);
    }
    if (m_trackerPickerButton) {
        m_trackerPickerButton->setEnabled(true);
    }
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
    applyTrackerSelectionToWorker();
    applyAiSettingsToWorker();

    m_camera->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_camera, &CameraWorker::run);
    connect(m_camera, &CameraWorker::frameReady, this, &MainWindow::updateFrame, Qt::QueuedConnection);
    connect(m_camera, &CameraWorker::finished, m_thread, &QThread::quit);
    connect(m_camera, &CameraWorker::videoInfo, this, &MainWindow::onVideoInfo, Qt::QueuedConnection);
    connect(m_camera, &CameraWorker::positionChanged, this, &MainWindow::onVideoPosition, Qt::QueuedConnection);
    connect(m_camera, &CameraWorker::playbackEnded, this, &MainWindow::onPlaybackEnded, Qt::QueuedConnection);
    connect(m_videoLabel, &VideoLabel::selectionMade, this, &MainWindow::onTrackerSelection);
    connect(m_videoLabel, &VideoLabel::trackerReset, this, &MainWindow::onTrackerReset);

    m_thread->start();

    if (m_isVideoMode) {
        onSpeedChanged(m_speedSlider->value());
    }
}

void MainWindow::applyAiSettingsToWorker() {
    if (!m_camera) {
        return;
    }

    const int interval = m_aiIntervalCombo ? m_aiIntervalCombo->currentData().toInt() : 30;
    m_camera->setYoloModel(m_yoloConfigPath, m_yoloWeightsPath, m_yoloClassNames);
    m_camera->setAiInterval(std::max(1, interval));
    m_camera->setAiEnabled(m_aiEnabledCheckBox && m_aiEnabledCheckBox->isChecked());
}

void MainWindow::refreshYoloStatusLabel() {
    if (!m_yoloStatusLabel) {
        return;
    }

    if (m_yoloConfigPath.isEmpty() || m_yoloWeightsPath.isEmpty()) {
        m_yoloStatusLabel->setText("YOLO: not loaded");
        return;
    }

    const QString modelName = QFileInfo(m_yoloWeightsPath).completeBaseName().isEmpty()
        ? QFileInfo(m_yoloConfigPath).completeBaseName()
        : QFileInfo(m_yoloWeightsPath).completeBaseName();
    const QString aiState = (m_aiEnabledCheckBox && m_aiEnabledCheckBox->isChecked()) ? "on" : "off";
    m_yoloStatusLabel->setText(QString("YOLO: %1 (%2)").arg(modelName, aiState));
}

QStringList MainWindow::loadClassNamesFromFile(const QString& filePath) const {
    QStringList classNames;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return classNames;
    }

    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (!line.isEmpty()) {
            classNames.push_back(line);
        }
    }

    return classNames;
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
    if (m_seekTimeLabel) {
        m_seekTimeLabel->setEnabled(enabled);
        if (!enabled) {
            updateSeekTimeLabel(0);
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
    if (m_eventsTextLabel) {
        m_eventsTextLabel->setEnabled(enabled);
    }
    if (m_eventsList) {
        m_eventsList->setEnabled(enabled);
    }
    if (m_removeEventButton) {
        m_removeEventButton->setEnabled(enabled);
    }
    if (m_clearEventsButton) {
        m_clearEventsButton->setEnabled(enabled);
    }
}

void MainWindow::updateFrame(const QImage& frame) {
    m_videoLabel->setPixmap(QPixmap::fromImage(frame));
    m_videoLabel->setFrameSize(frame.width(), frame.height());
}

int MainWindow::currentVideoFrame() const {
    if (!m_isVideoMode || !m_seekSlider) {
        return -1;
    }

    return m_seekSlider->value();
}

void MainWindow::updateSeekTimeLabel(int currentFrame) {
    if (!m_seekTimeLabel) {
        return;
    }

    const int safeCurrentFrame = std::max(0, currentFrame);
    const int totalFrameForTime = std::max(0, m_totalVideoFrames);
    m_seekTimeLabel->setText(QString("%1 / %2")
        .arg(frameToTimeText(safeCurrentFrame), frameToTimeText(totalFrameForTime)));
}

QStringList MainWindow::selectedTrackerTypes() const {
    QStringList selected;
    for (QAction* action : m_trackerActions) {
        if (action && action->isChecked()) {
            selected.push_back(action->text());
        }
    }

    if (selected.isEmpty()) {
        selected.push_back("CSRT");
    }
    return selected;
}

void MainWindow::syncTrackerButtonText() {
    if (!m_trackerPickerButton) {
        return;
    }

    const QStringList selected = selectedTrackerTypes();
    if (selected.size() == 1) {
        m_trackerPickerButton->setText(selected.first());
    } else {
        m_trackerPickerButton->setText(QString("%1 selected").arg(selected.size()));
    }
}

void MainWindow::applyTrackerSelectionToWorker() {
    if (!m_camera) {
        return;
    }

    m_camera->setTrackerTypes(selectedTrackerTypes());
}

void MainWindow::onTrackerActionToggled(bool checked) {
    Q_UNUSED(checked);

    bool hasCheckedAction = false;
    for (QAction* action : m_trackerActions) {
        if (action && action->isChecked()) {
            hasCheckedAction = true;
            break;
        }
    }

    if (!hasCheckedAction && !m_trackerActions.isEmpty()) {
        QAction* firstAction = m_trackerActions.first();
        if (firstAction) {
            firstAction->blockSignals(true);
            firstAction->setChecked(true);
            firstAction->blockSignals(false);
        }
    }

    syncTrackerButtonText();
    applyTrackerSelectionToWorker();
}

void MainWindow::applyTrackingEventForFrame(int frameIndex) {
    if (!m_isVideoMode || !m_camera || frameIndex < 0) {
        return;
    }

    if (frameIndex == m_lastAppliedTimelineFrame) {
        return;
    }

    const auto it = m_trackingTimeline.constFind(frameIndex);
    if (it == m_trackingTimeline.constEnd()) {
        m_lastAppliedTimelineFrame = frameIndex;
        return;
    }

    const TrackingEvent& event = it.value();
    if (event.type == TrackingEventType::SetRoi) {
        m_camera->initTracker(event.roi.x(), event.roi.y(), event.roi.width(), event.roi.height());
    } else {
        m_camera->resetTracker();
    }

    m_lastAppliedTimelineFrame = frameIndex;
}

void MainWindow::onRemoveEventClicked() {
    if (!m_eventsList) {
        return;
    }

    QListWidgetItem* selectedItem = m_eventsList->currentItem();
    if (!selectedItem) {
        return;
    }

    const int frameIndex = selectedItem->data(Qt::UserRole).toInt();
    m_trackingTimeline.remove(frameIndex);
    m_lastAppliedTimelineFrame = -1;
    refreshTrackingEventsUi();
}

void MainWindow::onClearEventsClicked() {
    if (m_trackingTimeline.isEmpty()) {
        return;
    }

    m_trackingTimeline.clear();
    m_lastAppliedTimelineFrame = -1;
    refreshTrackingEventsUi();
}

void MainWindow::onEventActivated(QListWidgetItem* item) {
    if (!item || !m_isVideoMode || !m_camera || !m_seekSlider) {
        return;
    }

    const int frameIndex = item->data(Qt::UserRole).toInt();
    m_seekSlider->setValue(frameIndex);
    m_camera->resetTracker();
    m_camera->seekToFrame(frameIndex);
    m_lastAppliedTimelineFrame = -1;
    applyTrackingEventForFrame(frameIndex);
}

void MainWindow::refreshTrackingEventsUi() {
    if (!m_eventsList) {
        return;
    }

    m_eventsList->clear();
    for (auto it = m_trackingTimeline.constBegin(); it != m_trackingTimeline.constEnd(); ++it) {
        auto* item = new QListWidgetItem(trackingEventText(it.key(), it.value()), m_eventsList);
        item->setData(Qt::UserRole, it.key());
    }
}

QString MainWindow::trackingEventText(int frameIndex, const TrackingEvent& event) const {
    const QString typeText = (event.type == TrackingEventType::SetRoi) ? "Set ROI" : "Stop";
    return QString("%1  |  %2").arg(frameToTimeText(frameIndex), typeText);
}

QString MainWindow::frameToTimeText(int frameIndex) const {
    const double fps = (m_videoFps > 0.0) ? m_videoFps : 30.0;
    const int totalMs = static_cast<int>((1000.0 * frameIndex) / fps);
    const int minutes = totalMs / 60000;
    const int seconds = (totalMs % 60000) / 1000;
    const int millis = totalMs % 1000;
    return QString("%1:%2.%3")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(millis, 3, 10, QChar('0'));
}
