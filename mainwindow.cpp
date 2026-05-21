#include "mainwindow.h"
#include "yoloassist.h"
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
#include <QFrame>
#include <QFont>
#include <QTextCursor>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Drone Tracker");
    resize(1240, 760);
    setMinimumSize(1024, 640);

    QFont uiFont("Segoe UI", 11);
    setFont(uiFont);

    auto* central = new QWidget(this);
    central->setObjectName("appRoot");
    setCentralWidget(central);

    m_videoLabel = new VideoLabel(this);
    m_videoLabel->setObjectName("videoSurface");
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumSize(320, 240);
    m_videoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_videoLabel->setScaledContents(false);
    m_debugOverlayLabel = new QTextEdit(m_videoLabel);
    m_debugOverlayLabel->setObjectName("debugOverlay");
    m_debugOverlayLabel->setReadOnly(true);
    m_debugOverlayLabel->setLineWrapMode(QTextEdit::NoWrap);
    m_debugOverlayLabel->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_debugOverlayLabel->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_debugOverlayLabel->hide();

    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* controlsCard = new QFrame(this);
    controlsCard->setObjectName("controlsCard");
    auto* controls = new QVBoxLayout;
    controls->setContentsMargins(12, 10, 12, 10);
    controls->setSpacing(8);
    auto* controlsTop = new QHBoxLayout;
    controlsTop->setSpacing(8);
    auto* controlsBottom = new QHBoxLayout;
    controlsBottom->setSpacing(8);
    auto* cameraButton = new QPushButton("Camera", this);
    auto* openVideoButton = new QPushButton("Open Video", this);
    m_exportVideoButton = new QPushButton("Export Video", this);
    m_exportYoloButton = new QPushButton("Export YOLO", this);
    m_playPauseButton = new QPushButton("Pause", this);
    m_resolutionTextLabel = new QLabel("Camera", this);
    m_resolutionCombo = new QComboBox(this);
    m_seekTextLabel = new QLabel("Seek", this);
    m_seekTimeLabel = new QLabel("00:00.000 / 00:00.000", this);
    m_speedTextLabel = new QLabel("Speed", this);
    m_speedValueLabel = new QLabel("1.00x", this);
    m_trackerTextLabel = new QLabel("Trackers", this);
    m_trackerPickerButton = new QPushButton(this);
    m_loadYoloButton = new QPushButton("Load ONNX", this);
    m_aiEnabledCheckBox = new QCheckBox("AI", this);
    m_aiIntervalCombo = new QComboBox(this);
    m_yoloStatusLabel = new QLabel("ONNX: not loaded", this);
    m_yoloStatusLabel->setObjectName("yoloStatusLabel");
    m_eventsTextLabel = new QLabel("Events", this);
    m_seekSlider = new QSlider(Qt::Horizontal, this);
    m_speedSlider = new QSlider(Qt::Horizontal, this);
    m_eventsList = new QListWidget(this);
    m_removeEventButton = new QPushButton("Remove", this);
    m_clearEventsButton = new QPushButton("Clear", this);

    cameraButton->setProperty("variant", "primary");
    openVideoButton->setProperty("variant", "primary");
    m_exportVideoButton->setProperty("variant", "accent");
    m_exportYoloButton->setProperty("variant", "accent");
    m_playPauseButton->setProperty("variant", "primary");
    m_loadYoloButton->setProperty("variant", "accent");
    m_removeEventButton->setProperty("variant", "flat");
    m_clearEventsButton->setProperty("variant", "flat");

    m_seekTextLabel->setProperty("caption", true);
    m_speedTextLabel->setProperty("caption", true);
    m_trackerTextLabel->setProperty("caption", true);
    m_eventsTextLabel->setProperty("caption", true);
    m_resolutionTextLabel->setProperty("caption", true);

    m_seekSlider->setRange(0, 0);
    m_seekSlider->setMinimumWidth(280);
    m_seekSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_seekTimeLabel->setMinimumWidth(180);
    m_speedSlider->setRange(25, 300);
    m_speedSlider->setValue(100);
    m_speedSlider->setMinimumWidth(170);
    m_resolutionCombo->addItem("640 x 480", QSize(640, 480));
    m_resolutionCombo->addItem("1280 x 720", QSize(1280, 720));
    m_resolutionCombo->addItem("1920 x 1080", QSize(1920, 1080));
    m_resolutionCombo->setCurrentIndex(0);
    m_resolutionCombo->setFixedWidth(138);
    m_trackerPickerButton->setMinimumWidth(132);
    m_loadYoloButton->setMinimumWidth(112);
    m_aiEnabledCheckBox->setChecked(false);
    m_aiIntervalCombo->addItem("5 frames", 5);
    m_aiIntervalCombo->addItem("10 frames", 10);
    m_aiIntervalCombo->addItem("20 frames", 20);
    m_aiIntervalCombo->addItem("30 frames", 30);
    m_aiIntervalCombo->addItem("60 frames", 60);
    m_aiIntervalCombo->setCurrentIndex(3);
    m_aiIntervalCombo->setFixedWidth(118);
    m_yoloStatusLabel->setMinimumWidth(195);
    m_eventsList->setMinimumWidth(220);
    m_eventsList->setMaximumWidth(260);
    m_removeEventButton->setFixedWidth(92);
    m_clearEventsButton->setFixedWidth(92);

    controlsTop->addWidget(cameraButton);
    controlsTop->addWidget(openVideoButton);
    controlsTop->addWidget(m_exportVideoButton);
    controlsTop->addWidget(m_exportYoloButton);
    controlsTop->addWidget(m_playPauseButton);
    controlsTop->addSpacing(8);
    controlsTop->addWidget(m_resolutionTextLabel);
    controlsTop->addWidget(m_resolutionCombo);
    controlsTop->addSpacing(8);
    controlsTop->addWidget(m_seekTextLabel);
    controlsTop->addWidget(m_seekSlider, 1);
    controlsTop->addWidget(m_seekTimeLabel);

    controlsBottom->addWidget(m_speedTextLabel);
    controlsBottom->addWidget(m_speedSlider);
    controlsBottom->addWidget(m_speedValueLabel);
    controlsBottom->addSpacing(16);
    controlsBottom->addWidget(m_trackerTextLabel);
    controlsBottom->addWidget(m_trackerPickerButton);
    controlsBottom->addWidget(m_loadYoloButton);
    controlsBottom->addSpacing(8);
    controlsBottom->addWidget(m_aiEnabledCheckBox);
    controlsBottom->addWidget(m_aiIntervalCombo);
    controlsBottom->addWidget(m_yoloStatusLabel);
    controlsBottom->addStretch();

    controls->addLayout(controlsTop);
    controls->addLayout(controlsBottom);
    controlsCard->setLayout(controls);

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

    layout->addWidget(controlsCard, 0);

    auto* contentLayout = new QHBoxLayout;
    contentLayout->setSpacing(12);

    auto* eventsCard = new QFrame(this);
    eventsCard->setObjectName("eventsCard");
    auto* eventsPanel = new QVBoxLayout(eventsCard);
    eventsPanel->setContentsMargins(12, 12, 12, 12);
    eventsPanel->setSpacing(8);
    eventsCard->setMinimumWidth(250);
    eventsCard->setMaximumWidth(320);

    eventsPanel->addWidget(m_eventsTextLabel, 0);
    eventsPanel->addWidget(m_eventsList, 1);

    auto* eventsButtons = new QHBoxLayout;
    eventsButtons->addWidget(m_removeEventButton);
    eventsButtons->addWidget(m_clearEventsButton);
    eventsButtons->addStretch();
    eventsPanel->addLayout(eventsButtons, 0);

    auto* videoCard = new QFrame(this);
    videoCard->setObjectName("videoCard");
    auto* videoLayout = new QVBoxLayout(videoCard);
    videoLayout->setContentsMargins(12, 12, 12, 12);
    videoLayout->setSpacing(6);
    videoLayout->addWidget(m_videoLabel, 1);

    contentLayout->addWidget(eventsCard, 0);
    contentLayout->addWidget(videoCard, 1);
    layout->addLayout(contentLayout, 1);

    setStyleSheet(
        "QWidget#appRoot {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #070d15, stop:1 #0f1822);"
        "  color: #dfeaf5;"
        "}"
        "QFrame#controlsCard, QFrame#eventsCard, QFrame#videoCard {"
        "  background: rgba(9, 15, 24, 0.82);"
        "  border: 1px solid rgba(168, 198, 226, 0.28);"
        "  border-radius: 6px;"
        "}"
        "QLabel[caption=\"true\"] {"
        "  color: #a9c7e2;"
        "  font-weight: 700;"
        "  letter-spacing: 0.5px;"
        "}"
        "QLabel[caption=\"true\"]:disabled {"
        "  color: #91adc7;"
        "}"
        "QLabel:disabled {"
        "  color: #a8bdd0;"
        "}"
        "QLabel#yoloStatusLabel {"
        "  color: #f0d9a1;"
        "  font-weight: 700;"
        "}"
        "QLabel#videoSurface {"
        "  background-color: #06090f;"
        "  border: 1px solid rgba(200, 220, 238, 0.45);"
        "  border-radius: 2px;"
        "}"
        "QTextEdit#debugOverlay {"
        "  background: rgba(5, 10, 16, 0.72);"
        "  color: #ecf6ff;"
        "  border: 1px solid rgba(170, 208, 235, 0.55);"
        "  border-radius: 4px;"
        "  font-family: Consolas, 'Courier New', monospace;"
        "  font-size: 9.5pt;"
        "  padding: 4px;"
        "}"
        "QTextEdit#debugOverlay QScrollBar:vertical {"
        "  background: rgba(10, 18, 28, 0.8);"
        "  width: 12px;"
        "  margin: 2px;"
        "}"
        "QTextEdit#debugOverlay QScrollBar::handle:vertical {"
        "  background: rgba(150, 193, 224, 0.65);"
        "  min-height: 20px;"
        "  border-radius: 4px;"
        "}"
        "QTextEdit#debugOverlay QScrollBar:horizontal {"
        "  background: rgba(10, 18, 28, 0.8);"
        "  height: 12px;"
        "  margin: 2px;"
        "}"
        "QTextEdit#debugOverlay QScrollBar::handle:horizontal {"
        "  background: rgba(150, 193, 224, 0.65);"
        "  min-width: 20px;"
        "  border-radius: 4px;"
        "}"
        "QTextEdit#debugOverlay QScrollBar::add-line, QTextEdit#debugOverlay QScrollBar::sub-line {"
        "  width: 0px;"
        "  height: 0px;"
        "}"
        "QPushButton {"
        "  border-radius: 3px;"
        "  border: 1px solid rgba(174, 205, 232, 0.35);"
        "  background: rgba(30, 49, 69, 0.92);"
        "  color: #eff7ff;"
        "  padding: 4px 10px;"
        "  min-height: 24px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(49, 76, 103, 0.94);"
        "}"
        "QPushButton:pressed {"
        "  background: rgba(24, 39, 54, 0.98);"
        "}"
        "QPushButton[variant=\"primary\"] {"
        "  background: rgba(26, 88, 119, 0.95);"
        "  border-color: rgba(158, 218, 248, 0.7);"
        "}"
        "QPushButton[variant=\"primary\"]:hover {"
        "  background: rgba(34, 108, 145, 0.97);"
        "}"
        "QPushButton[variant=\"accent\"] {"
        "  background: rgba(124, 90, 39, 0.95);"
        "  border-color: rgba(238, 199, 129, 0.78);"
        "}"
        "QPushButton[variant=\"accent\"]:hover {"
        "  background: rgba(146, 107, 49, 0.96);"
        "}"
        "QPushButton[variant=\"flat\"] {"
        "  background: rgba(28, 41, 57, 0.9);"
        "}"
        "QComboBox, QListWidget, QSlider, QCheckBox, QLabel {"
        "  font-size: 10.5pt;"
        "}"
        "QComboBox, QListWidget {"
        "  background: rgba(15, 27, 39, 0.96);"
        "  color: #f2f7ff;"
        "  border: 1px solid rgba(171, 202, 230, 0.35);"
        "  border-radius: 3px;"
        "  padding: 4px 8px;"
        "  min-height: 24px;"
        "}"
        "QComboBox:disabled {"
        "  color: #aec1d3;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  width: 20px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: #0f1e2d;"
        "  color: #f2f7ff;"
        "  selection-background-color: #3e7395;"
        "  selection-color: #ffffff;"
        "  border: 1px solid rgba(174, 205, 232, 0.45);"
        "  outline: 0;"
        "}"
        "QListWidget::item:selected {"
        "  background: #376786;"
        "  border-radius: 2px;"
        "}"
        "QCheckBox {"
        "  spacing: 6px;"
        "}"
        "QCheckBox::indicator {"
        "  width: 15px;"
        "  height: 15px;"
        "  border-radius: 2px;"
        "  border: 1px solid rgba(171, 202, 230, 0.55);"
        "  background: #11202f;"
        "}"
        "QCheckBox::indicator:checked {"
        "  background: #3ca27f;"
        "  border: 1px solid #8ce6c9;"
        "}"
        "QSlider::groove:horizontal {"
        "  border: none;"
        "  height: 4px;"
        "  border-radius: 1px;"
        "  background: rgba(84, 112, 136, 0.5);"
        "}"
        "QSlider::sub-page:horizontal {"
        "  border-radius: 1px;"
        "  background: #79c9f6;"
        "}"
        "QSlider::handle:horizontal {"
        "  width: 10px;"
        "  margin: -5px 0;"
        "  border-radius: 5px;"
        "  background: #f5fbff;"
        "  border: 1px solid #9bc7e3;"
        "}"
    );

    connect(cameraButton, &QPushButton::clicked, this, &MainWindow::onCameraClicked);
    connect(openVideoButton, &QPushButton::clicked, this, &MainWindow::onOpenVideoClicked);
    connect(m_exportVideoButton, &QPushButton::clicked, this, &MainWindow::onExportVideoClicked);
    connect(m_exportYoloButton, &QPushButton::clicked, this, &MainWindow::onExportYoloClicked);
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

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (event && event->key() == Qt::Key_F3) {
        m_debugOverlayEnabled = !m_debugOverlayEnabled;
        if (m_camera) {
            m_camera->setDebugEnabled(m_debugOverlayEnabled);
        }

        if (!m_debugOverlayEnabled && m_debugOverlayLabel) {
            m_debugOverlayLabel->clear();
            m_debugOverlayLabel->hide();
        }

        event->accept();
        return;
    }

    QMainWindow::keyPressEvent(event);
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
    bool exportAiAssistEnabled = (m_aiEnabledCheckBox && m_aiEnabledCheckBox->isChecked() && !m_yoloModelPath.isEmpty());
    const int aiIntervalFrames = std::max(1, m_aiIntervalCombo ? m_aiIntervalCombo->currentData().toInt() : 30);
    int targetClassId = -1;
    bool targetClassPending = false;
    YoloAssist exportYoloAssist;
    if (exportAiAssistEnabled) {
        exportYoloAssist.setModel(m_yoloModelPath, m_yoloClassNames);
    }

    auto applyTrackerBoxAndReinitialize = [&](const cv::Rect2d& box, const cv::Mat& currentFrame) {
        cv::Rect initRect = static_cast<cv::Rect>(box);
        initRect.x = std::max(0, std::min(initRect.x, std::max(0, currentFrame.cols - 1)));
        initRect.y = std::max(0, std::min(initRect.y, std::max(0, currentFrame.rows - 1)));
        initRect.width = std::max(1, std::min(initRect.width, std::max(1, currentFrame.cols - initRect.x)));
        initRect.height = std::max(1, std::min(initRect.height, std::max(1, currentFrame.rows - initRect.y)));

        trackerBox = cv::Rect2d(initRect.x, initRect.y, initRect.width, initRect.height);
        trackers = buildTrackers(selectedTypes);
        trackingInitialized = false;

        int initCount = 0;
        for (auto& tracker : trackers) {
            if (!tracker) {
                continue;
            }
            try {
                tracker->init(currentFrame, initRect);
                ++initCount;
            } catch (const cv::Exception& ex) {
                qWarning() << "Export tracker reinit failed after YOLO correction:" << ex.what();
            }
        }

        if (initCount == 0) {
            try {
                trackers.clear();
                cv::Ptr<cv::Tracker> fallback = cv::TrackerCSRT::create();
                fallback->init(currentFrame, initRect);
                trackers.push_back(fallback);
                initCount = 1;
            } catch (const cv::Exception& ex) {
                qWarning() << "Export fallback tracker reinit failed after YOLO correction:" << ex.what();
            }
        }

        trackingInitialized = (initCount > 0);
    };

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
                if (exportAiAssistEnabled) {
                    targetClassId = -1;
                    targetClassPending = true;
                }
            } else {
                trackers.clear();
                trackingActive = false;
                trackingInitialized = false;
                targetClassId = -1;
                targetClassPending = false;
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

            const bool runAiPass = exportAiAssistEnabled && (targetClassPending || (aiIntervalFrames > 0 && frameIndex % aiIntervalFrames == 0));
            if (runAiPass) {
                std::vector<YoloAssist::Detection> detections;
                QString yoloError;
                if (exportYoloAssist.detect(frame, detections, &yoloError) && !detections.empty()) {
                    YoloAssist::Detection selectedDetection;
                    bool foundDetection = false;

                    if (targetClassPending) {
                        foundDetection = YoloAssist::chooseDetectionForSelection(detections, trackerBox, selectedDetection);
                    }

                    if (!foundDetection && targetClassId >= 0) {
                        std::vector<YoloAssist::Detection> matchingDetections;
                        for (const YoloAssist::Detection& detection : detections) {
                            if (detection.classId == targetClassId) {
                                matchingDetections.push_back(detection);
                            }
                        }

                        if (!matchingDetections.empty()) {
                            foundDetection = YoloAssist::chooseDetectionForCorrection(matchingDetections, trackerBox, selectedDetection);
                        }
                    }

                    if (!foundDetection) {
                        foundDetection = YoloAssist::chooseDetectionForCorrection(detections, trackerBox, selectedDetection);
                    }

                    if (foundDetection) {
                        applyTrackerBoxAndReinitialize(
                            cv::Rect2d(selectedDetection.box.x, selectedDetection.box.y, selectedDetection.box.width, selectedDetection.box.height),
                            frame);

                        if (targetClassPending) {
                            targetClassId = selectedDetection.classId;
                            targetClassPending = false;
                        }
                    }
                } else if (!yoloError.isEmpty()) {
                    qWarning() << "Export YOLO inference failed; disabling export AI assist:" << yoloError;
                    exportAiAssistEnabled = false;
                    targetClassPending = false;
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

void MainWindow::onExportYoloClicked() {
    if (!m_isVideoMode || m_currentVideoPath.isEmpty()) {
        QMessageBox::information(this, "Export YOLO", "Open a video file first to export YOLO annotations.");
        return;
    }

    const QFileInfo inputInfo(m_currentVideoPath);
    const QString defaultDir = inputInfo.absolutePath();
    const QString outDir = QFileDialog::getExistingDirectory(this, "Select output folder for YOLO annotations", defaultDir);
    if (outDir.isEmpty()) {
        return;
    }

    const QString baseDir = outDir + "/" + inputInfo.completeBaseName() + "_yolo";
    const QString imagesDir = baseDir + "/images";
    const QString labelsDir = baseDir + "/labels";
    QDir dir;
    if (!dir.mkpath(imagesDir)) {
        QMessageBox::warning(this, "Export YOLO", "Failed to create images output directory: " + imagesDir);
        return;
    }
    if (!dir.mkpath(labelsDir)) {
        QMessageBox::warning(this, "Export YOLO", "Failed to create labels output directory: " + labelsDir);
        return;
    }

    cv::VideoCapture cap(m_currentVideoPath.toStdString());
    if (!cap.isOpened()) {
        QMessageBox::warning(this, "Export YOLO", "Failed to open the source video for export.");
        return;
    }

    const int frameWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    const int frameHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    if (frameWidth <= 0 || frameHeight <= 0) {
        QMessageBox::warning(this, "Export YOLO", "Invalid source video dimensions.");
        return;
    }

    const int totalFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    QProgressDialog progress("Exporting YOLO labels...", "Cancel", 0, std::max(0, totalFrames), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    const QStringList selectedTypes = selectedTrackerTypes();
    auto createTrackerByName = [](const QString& trackerName) -> cv::Ptr<cv::Tracker> {
        const QString name = trackerName.trimmed();
        if (name.compare("CSRT", Qt::CaseInsensitive) == 0) return cv::TrackerCSRT::create();
        if (name.compare("KCF", Qt::CaseInsensitive) == 0) return cv::TrackerKCF::create();
        if (name.compare("MIL", Qt::CaseInsensitive) == 0) return cv::TrackerMIL::create();
        if (name.compare("GOTURN", Qt::CaseInsensitive) == 0) return cv::TrackerGOTURN::create();
        if (name.compare("DaSiamRPN", Qt::CaseInsensitive) == 0) return cv::TrackerDaSiamRPN::create();
        if (name.compare("Nano", Qt::CaseInsensitive) == 0) return cv::TrackerNano::create();
        if (name.compare("Vit", Qt::CaseInsensitive) == 0) return cv::TrackerVit::create();
        return {};
    };

    auto buildTrackers = [&](const QStringList& selectedTypes) {
        std::vector<cv::Ptr<cv::Tracker>> trackers;
        for (const QString& type : selectedTypes) {
            try {
                cv::Ptr<cv::Tracker> tracker = createTrackerByName(type);
                if (tracker) trackers.push_back(tracker);
            } catch (const cv::Exception& ex) {
                qWarning() << "Failed to create tracker" << type << ":" << ex.what();
            }
        }
        if (trackers.empty()) trackers.push_back(cv::TrackerCSRT::create());
        return trackers;
    };

    std::vector<cv::Ptr<cv::Tracker>> trackers;
    bool trackingActive = false;
    bool trackingInitialized = false;
    cv::Rect2d trackerBox;
    bool exportAiAssistEnabled = (m_aiEnabledCheckBox && m_aiEnabledCheckBox->isChecked() && !m_yoloModelPath.isEmpty());
    const int aiIntervalFrames = std::max(1, m_aiIntervalCombo ? m_aiIntervalCombo->currentData().toInt() : 30);
    int targetClassId = -1;
    bool targetClassPending = false;
    YoloAssist exportYoloAssist;
    if (exportAiAssistEnabled) {
        exportYoloAssist.setModel(m_yoloModelPath, m_yoloClassNames);
    }

    auto applyTrackerBoxAndReinitialize = [&](const cv::Rect2d& box, const cv::Mat& currentFrame) {
        cv::Rect initRect = static_cast<cv::Rect>(box);
        initRect.x = std::max(0, std::min(initRect.x, std::max(0, currentFrame.cols - 1)));
        initRect.y = std::max(0, std::min(initRect.y, std::max(0, currentFrame.rows - 1)));
        initRect.width = std::max(1, std::min(initRect.width, std::max(1, currentFrame.cols - initRect.x)));
        initRect.height = std::max(1, std::min(initRect.height, std::max(1, currentFrame.rows - initRect.y)));

        trackerBox = cv::Rect2d(initRect.x, initRect.y, initRect.width, initRect.height);
        trackers = buildTrackers(selectedTypes);
        trackingInitialized = false;

        int initCount = 0;
        for (auto& tracker : trackers) {
            if (!tracker) continue;
            try {
                tracker->init(currentFrame, initRect);
                ++initCount;
            } catch (const cv::Exception& ex) {
                qWarning() << "Export (YOLO) tracker reinit failed:" << ex.what();
            }
        }

        if (initCount == 0) {
            try {
                trackers.clear();
                cv::Ptr<cv::Tracker> fallback = cv::TrackerCSRT::create();
                fallback->init(currentFrame, initRect);
                trackers.push_back(fallback);
                initCount = 1;
            } catch (const cv::Exception& ex) {
                qWarning() << "Export (YOLO) fallback tracker reinit failed:" << ex.what();
            }
        }

        trackingInitialized = (initCount > 0);
    };

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
                if (exportAiAssistEnabled) {
                    targetClassId = -1;
                    targetClassPending = true;
                }
            } else {
                trackers.clear();
                trackingActive = false;
                trackingInitialized = false;
                targetClassId = -1;
                targetClassPending = false;
            }
        }

        if (trackingActive && !trackers.empty()) {
            if (!trackingInitialized) {
                const cv::Rect initRect = static_cast<cv::Rect>(trackerBox);
                int initCount = 0;
                for (auto& tracker : trackers) {
                    if (!tracker) continue;
                    try {
                        tracker->init(frame, initRect);
                        ++initCount;
                    } catch (const cv::Exception& ex) {
                        qWarning() << "Export (YOLO) tracker init failed:" << ex.what();
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
                        qWarning() << "Export (YOLO) fallback tracker init failed:" << ex.what();
                    }
                }
                trackingInitialized = (initCount > 0);
            } else {
                double sumX = 0.0, sumY = 0.0, sumW = 0.0, sumH = 0.0;
                int successCount = 0;
                for (auto& tracker : trackers) {
                    if (!tracker) continue;
                    cv::Rect currentBox = static_cast<cv::Rect>(trackerBox);
                    bool updated = false;
                    try {
                        updated = tracker->update(frame, currentBox);
                    } catch (const cv::Exception& ex) {
                        qWarning() << "Export (YOLO) tracker update failed:" << ex.what();
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

            const bool runAiPass = exportAiAssistEnabled && (targetClassPending || (aiIntervalFrames > 0 && frameIndex % aiIntervalFrames == 0));
            if (runAiPass) {
                std::vector<YoloAssist::Detection> detections;
                QString yoloError;
                if (exportYoloAssist.detect(frame, detections, &yoloError) && !detections.empty()) {
                    YoloAssist::Detection selectedDetection;
                    bool foundDetection = false;
                    if (targetClassPending) {
                        foundDetection = YoloAssist::chooseDetectionForSelection(detections, trackerBox, selectedDetection);
                    }
                    if (!foundDetection && targetClassId >= 0) {
                        std::vector<YoloAssist::Detection> matchingDetections;
                        for (const YoloAssist::Detection& detection : detections) {
                            if (detection.classId == targetClassId) matchingDetections.push_back(detection);
                        }
                        if (!matchingDetections.empty()) {
                            foundDetection = YoloAssist::chooseDetectionForCorrection(matchingDetections, trackerBox, selectedDetection);
                        }
                    }
                    if (!foundDetection) {
                        foundDetection = YoloAssist::chooseDetectionForCorrection(detections, trackerBox, selectedDetection);
                    }
                    if (foundDetection) {
                        applyTrackerBoxAndReinitialize(
                            cv::Rect2d(selectedDetection.box.x, selectedDetection.box.y, selectedDetection.box.width, selectedDetection.box.height),
                            frame);
                        if (targetClassPending) {
                            targetClassId = selectedDetection.classId;
                            targetClassPending = false;
                        }
                    }
                } else if (!yoloError.isEmpty()) {
                    qWarning() << "Export (YOLO) inference failed; disabling export AI assist:" << yoloError;
                    exportAiAssistEnabled = false;
                    targetClassPending = false;
                }
            }
        }

        // Write frame as image for this frame
        const QString imageFile = QString("%1/frame_%2.png").arg(imagesDir).arg(frameIndex, 6, 10, QChar('0'));
        try {
            cv::imwrite(imageFile.toStdString(), frame);
        } catch (const cv::Exception& ex) {
            qWarning() << "Failed to save frame image:" << ex.what();
        }

        // Write YOLO label file for this frame (one file per frame). If trackingInitialized, write the consensus box.
        const QString labelFile = QString("%1/frame_%2.txt").arg(labelsDir).arg(frameIndex, 6, 10, QChar('0'));
        QFile file(labelFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            if (trackingInitialized) {
                // Normalize and write: class_id x_center y_center width height
                const double x = trackerBox.x;
                const double y = trackerBox.y;
                const double w = trackerBox.width;
                const double h = trackerBox.height;
                const double xCenter = (x + w / 2.0) / static_cast<double>(frameWidth);
                const double yCenter = (y + h / 2.0) / static_cast<double>(frameHeight);
                const double wN = w / static_cast<double>(frameWidth);
                const double hN = h / static_cast<double>(frameHeight);
                const int classIdToWrite = (targetClassId >= 0) ? targetClassId : 0;
                out.setRealNumberPrecision(6);
                out << classIdToWrite << " " << xCenter << " " << yCenter << " " << wN << " " << hN << "\n";
            }
            file.close();
        }

        ++frameIndex;
        progress.setValue(frameIndex);
        QCoreApplication::processEvents();
    }

    cap.release();

    if (canceled) {
        QMessageBox::information(this, "Export YOLO", "Export canceled.");
        return;
    }

    progress.setValue(std::max(totalFrames, frameIndex));
    QMessageBox::information(this, "Export YOLO", "YOLO data exported to:\n" + baseDir);
}

void MainWindow::onLoadYoloModelClicked() {
    const QString configPath = QFileDialog::getOpenFileName(
        this,
        "Open ONNX Model",
        QString(),
        "ONNX Model (*.onnx);;All Files (*.*)");
    if (configPath.isEmpty()) {
        return;
    }

    const QString namesPath = QFileDialog::getOpenFileName(
        this,
        "Open Class Names (optional)",
        QFileInfo(configPath).absolutePath(),
        "Class Names (*.names *.txt);;All Files (*.*)");

    m_yoloModelPath = configPath;
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
    connect(m_camera, &CameraWorker::debugInfoReady, this, &MainWindow::onDebugInfoReady, Qt::QueuedConnection);
    connect(m_camera, &CameraWorker::finished, m_thread, &QThread::quit);
    connect(m_camera, &CameraWorker::videoInfo, this, &MainWindow::onVideoInfo, Qt::QueuedConnection);
    connect(m_camera, &CameraWorker::positionChanged, this, &MainWindow::onVideoPosition, Qt::QueuedConnection);
    connect(m_camera, &CameraWorker::playbackEnded, this, &MainWindow::onPlaybackEnded, Qt::QueuedConnection);
    connect(m_videoLabel, &VideoLabel::selectionMade, this, &MainWindow::onTrackerSelection);
    connect(m_videoLabel, &VideoLabel::trackerReset, this, &MainWindow::onTrackerReset);

    m_camera->setDebugEnabled(m_debugOverlayEnabled);

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
    m_camera->setYoloModel(m_yoloModelPath, m_yoloClassNames);
    m_camera->setAiInterval(std::max(1, interval));
    m_camera->setAiEnabled(m_aiEnabledCheckBox && m_aiEnabledCheckBox->isChecked());
}

void MainWindow::refreshYoloStatusLabel() {
    if (!m_yoloStatusLabel) {
        return;
    }

    if (m_yoloModelPath.isEmpty()) {
        const QString aiState = (m_aiEnabledCheckBox && m_aiEnabledCheckBox->isChecked()) ? "ON" : "OFF";
        m_yoloStatusLabel->setText(QString("ONNX: not loaded | AI: %1").arg(aiState));
        return;
    }

    const QString modelName = QFileInfo(m_yoloModelPath).completeBaseName();
    const QString aiState = (m_aiEnabledCheckBox && m_aiEnabledCheckBox->isChecked()) ? "ON" : "OFF";
    m_yoloStatusLabel->setText(QString("ONNX: %1 | AI: %2").arg(modelName, aiState));
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

    if (m_debugOverlayLabel && !m_debugOverlayEnabled) {
        m_debugOverlayLabel->clear();
        m_debugOverlayLabel->hide();
    }
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
    QPixmap pixmap = QPixmap::fromImage(frame);
    const QSize targetSize = m_videoLabel->contentsRect().size();
    if (targetSize.isValid()) {
        pixmap = pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    m_videoLabel->setPixmap(pixmap);
    m_videoLabel->setFrameSize(frame.width(), frame.height());

    if (m_debugOverlayLabel) {
        const QRect area = m_videoLabel->contentsRect();
        const int overlayWidth = std::min(520, std::max(260, area.width() / 2));
        const int overlayHeight = std::min(320, std::max(120, area.height() / 3));
        m_debugOverlayLabel->setGeometry(area.left() + 12, area.top() + 12, overlayWidth, overlayHeight);
        m_debugOverlayLabel->setVisible(m_debugOverlayEnabled && !m_debugOverlayLabel->toPlainText().isEmpty());
        m_debugOverlayLabel->raise();
    }
}

void MainWindow::onDebugInfoReady(const QString& info) {
    if (!m_debugOverlayLabel) {
        return;
    }

    if (!m_debugOverlayEnabled || info.isEmpty()) {
        m_debugOverlayLabel->clear();
        m_debugOverlayLabel->hide();
        m_debugScrollPosition = 0;
        return;
    }

    // Save scroll position and cursor position
    const int scrollPos = m_debugOverlayLabel->verticalScrollBar()->value();
    
    // Block signals to prevent automatic scrolling
    const bool wasBlocked = m_debugOverlayLabel->blockSignals(true);
    m_debugOverlayLabel->verticalScrollBar()->blockSignals(true);
    
    // Use lower-level document cursor operations instead of setPlainText
    QTextCursor cursor(m_debugOverlayLabel->document());
    cursor.beginEditBlock();
    cursor.select(QTextCursor::Document);
    cursor.insertText(info);
    cursor.endEditBlock();
    
    // Restore signal blocking
    m_debugOverlayLabel->blockSignals(wasBlocked);
    m_debugOverlayLabel->verticalScrollBar()->blockSignals(false);
    
    // Restore scroll position
    m_debugOverlayLabel->verticalScrollBar()->setValue(scrollPos);
    
    m_debugOverlayLabel->show();
    m_debugOverlayLabel->raise();
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
