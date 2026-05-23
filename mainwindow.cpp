#include "mainwindow.h"
#include "yoloassist.h"
#include "timeline_repository.h"
#include "application_settings.h"
#include "tracking_manager.h"
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
#include <QDockWidget>
#include <QToolBar>
#include <QSettings>
#include <QApplication>
#include <QScreen>
#include <QToolTip>
#include <QCursor>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Drone Tracker");
    resize(1240, 760);
    setMinimumSize(1290, 580);  // Account for all docks: video(640) + events(350) + settings(300) + spacing

    QFont uiFont("Segoe UI", 11);
    setFont(uiFont);

    // Initialize Phase 1 components
    m_timeline = new TimelineRepository(this);
    m_settings = new ApplicationSettings(this);

    // Create central video widget - maximized by default
    m_videoLabel = new VideoLabel(this);
    m_videoLabel->setObjectName("videoSurface");
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumSize(640, 480);
    m_videoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_videoLabel->setScaledContents(false);
    m_debugOverlay = new DebugOverlayRenderer(m_videoLabel);
    setCentralWidget(m_videoLabel);

    // Create all widgets first
    createWidgetComponents();

    // Setup UI components in proper order
    setupMenuBar();
    setupDockWidgets();
    setupPlaybackToolbar();

    // Apply styling
    applyStylesheet();

    // Create connections
    createConnections();

    // Initialize state
    setVideoControlsEnabled(false);
    refreshYoloStatusLabel();
    applyAiSettingsToWorker();

    // Restore window state
    restoreWindowState();

    startSource(buildCameraPipeline(), true);
}

void MainWindow::createWidgetComponents() {
    // Playback controls - using SeekBar widget
    m_seekBar = new SeekBar(this);
    m_seekBar->setMetadata(0, 30.0);

    // Labels and controls
    m_resolutionTextLabel = new QLabel("Camera", this);
    m_resolutionCombo = new QComboBox(this);
    m_speedTextLabel = new QLabel("Speed", this);
    m_speedValueLabel = new QLabel("1.00x", this);
    m_trackerTextLabel = new QLabel("Trackers", this);
    m_trackerPickerButton = new QPushButton(this);
    m_loadYoloButton = new QPushButton("Load ONNX", this);
    m_aiEnabledCheckBox = new QCheckBox("AI", this);
    m_aiIntervalCombo = new QComboBox(this);
    m_yoloStatusLabel = new QLabel("ONNX: not loaded", this);
    m_yoloStatusLabel->setObjectName("yoloStatusLabel");

    // Sliders and widgets
    m_speedSlider = new QSlider(Qt::Horizontal, this);
    m_eventsList = new QListWidget(this);
    m_removeEventButton = new QPushButton("Remove Event", this);
    m_clearEventsButton = new QPushButton("Clear All", this);

    // Button style variants
    m_loadYoloButton->setProperty("variant", "accent");
    m_removeEventButton->setProperty("variant", "flat");
    m_clearEventsButton->setProperty("variant", "flat");

    // Label captions
    m_speedTextLabel->setProperty("caption", true);
    m_trackerTextLabel->setProperty("caption", true);
    m_resolutionTextLabel->setProperty("caption", true);

    // Configure seekBar
    m_seekBar->setRange(0, 0);
    m_seekBar->setPlayButtonText("Play");

    // Configure speedSlider
    m_speedSlider->setRange(25, 300);
    m_speedSlider->setValue(100);
    m_speedSlider->setMinimumWidth(170);

    // Configure resolution combo
    m_resolutionCombo->addItem("640 x 480", QSize(640, 480));
    m_resolutionCombo->addItem("1280 x 720", QSize(1280, 720));
    m_resolutionCombo->addItem("1920 x 1080", QSize(1920, 1080));
    m_resolutionCombo->setCurrentIndex(0);
    m_resolutionCombo->setFixedWidth(175);

    // Configure tracker button
    m_trackerPickerButton->setMinimumWidth(132);
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

    // Configure YOLO button
    m_loadYoloButton->setMinimumWidth(112);

    // Configure AI checkbox and combo - DISABLED until model loads
    m_aiEnabledCheckBox->setChecked(false);
    m_aiEnabledCheckBox->setEnabled(false);  // Disabled until model loaded
    m_aiIntervalCombo->addItem("5 frames", 5);
    m_aiIntervalCombo->addItem("10 frames", 10);
    m_aiIntervalCombo->addItem("20 frames", 20);
    m_aiIntervalCombo->addItem("30 frames", 30);
    m_aiIntervalCombo->addItem("60 frames", 60);
    m_aiIntervalCombo->setCurrentIndex(3);
    m_aiIntervalCombo->setFixedWidth(150);
    m_aiIntervalCombo->setEnabled(false);  // Disabled until model loaded

    // Configure events list
    m_eventsList->setMinimumWidth(320);
    m_removeEventButton->setMinimumHeight(36);
    m_clearEventsButton->setMinimumHeight(36);
    m_yoloStatusLabel->setMinimumWidth(195);
}

void MainWindow::setupMenuBar() {
    m_menuBar = menuBar();

    // File Menu
    m_fileMenu = m_menuBar->addMenu("&File");
    m_actionOpenVideo = m_fileMenu->addAction("&Open Video");
    m_actionOpenVideo->setShortcut(Qt::CTRL | Qt::Key_O);
    connect(m_actionOpenVideo, &QAction::triggered, this, &MainWindow::onMenuOpenVideo);

    m_actionOpenCamera = m_fileMenu->addAction("Open &Camera");
    m_actionOpenCamera->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_O);
    connect(m_actionOpenCamera, &QAction::triggered, this, &MainWindow::onMenuOpenCamera);

    m_fileMenu->addSeparator();
    m_actionExit = m_fileMenu->addAction("E&xit");
    m_actionExit->setShortcut(Qt::CTRL | Qt::Key_Q);
    connect(m_actionExit, &QAction::triggered, this, &MainWindow::onMenuExit);

    // Playback Menu
    m_playbackMenu = m_menuBar->addMenu("&Playback");
    m_actionPlayPause = m_playbackMenu->addAction("&Play/Pause");
    m_actionPlayPause->setShortcut(Qt::Key_Space);
    connect(m_actionPlayPause, &QAction::triggered, this, &MainWindow::onMenuPlayPause);

    m_actionStop = m_playbackMenu->addAction("&Stop");
    m_actionStop->setShortcut(Qt::SHIFT | Qt::Key_Space);
    connect(m_actionStop, &QAction::triggered, this, &MainWindow::onMenuStop);

    m_playbackMenu->addSeparator();
    m_speedSubmenu = m_playbackMenu->addMenu("&Speed");
    for (double speed : {0.5, 0.75, 1.0, 1.5, 2.0}) {
        QAction* speedAction = m_speedSubmenu->addAction(QString::number(speed, 'f', 2) + "x");
        speedAction->setData(static_cast<int>(speed * 100.0));
        connect(speedAction, &QAction::triggered, this, [this, speed]() {
            m_speedSlider->setValue(static_cast<int>(speed * 100.0));
        });
    }

    // View Menu
    m_viewMenu = m_menuBar->addMenu("&View");
    m_actionToggleEvents = m_viewMenu->addAction("&Events Panel");
    m_actionToggleEvents->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_E);
    m_actionToggleEvents->setCheckable(true);
    m_actionToggleEvents->setChecked(false);
    connect(m_actionToggleEvents, &QAction::triggered, this, &MainWindow::onMenuToggleEvents);

    m_actionToggleSettings = m_viewMenu->addAction("&Settings Panel");
    m_actionToggleSettings->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_S);
    m_actionToggleSettings->setCheckable(true);
    m_actionToggleSettings->setChecked(false);
    connect(m_actionToggleSettings, &QAction::triggered, this, &MainWindow::onMenuToggleSettings);

    m_actionToggleDebug = m_viewMenu->addAction("&Debug Overlay");
    m_actionToggleDebug->setShortcut(Qt::Key_F3);
    m_actionToggleDebug->setCheckable(true);
    m_actionToggleDebug->setChecked(false);
    connect(m_actionToggleDebug, &QAction::triggered, this, &MainWindow::onMenuToggleDebug);

    m_viewMenu->addSeparator();
    m_actionFullscreen = m_viewMenu->addAction("&Fullscreen");
    m_actionFullscreen->setShortcut(Qt::Key_F11);
    connect(m_actionFullscreen, &QAction::triggered, this, &MainWindow::onMenuFullscreen);

    // Tools Menu
    m_toolsMenu = m_menuBar->addMenu("&Tools");
    m_actionLoadYolo = m_toolsMenu->addAction("&Load YOLO Model");
    m_actionLoadYolo->setShortcut(Qt::Key_L);
    connect(m_actionLoadYolo, &QAction::triggered, this, &MainWindow::onMenuLoadYolo);

    m_actionTrackerSettings = m_toolsMenu->addAction("&Tracker Selection");
    m_actionTrackerSettings->setShortcut(Qt::Key_T);
    connect(m_actionTrackerSettings, &QAction::triggered, this, &MainWindow::onMenuTrackerSettings);

    m_toolsMenu->addSeparator();
    m_actionExportVideo = m_toolsMenu->addAction("Export &Video");
    m_actionExportVideo->setShortcut(Qt::CTRL | Qt::Key_E);
    connect(m_actionExportVideo, &QAction::triggered, this, &MainWindow::onMenuExportVideo);

    m_actionExportYolo = m_toolsMenu->addAction("Export &YOLO");
    m_actionExportYolo->setShortcut(Qt::CTRL | Qt::Key_Y);
    connect(m_actionExportYolo, &QAction::triggered, this, &MainWindow::onMenuExportYolo);
}

void MainWindow::setupDockWidgets() {
    // Events Dock
    m_eventsDock = new QDockWidget("Events", this);
    m_eventsDock->setObjectName("EventsDock");
    m_eventsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto* eventsWidget = new QWidget(this);
    auto* eventsLayout = new QVBoxLayout(eventsWidget);
    eventsLayout->setContentsMargins(8, 8, 8, 8);
    eventsLayout->setSpacing(8);
    eventsLayout->addWidget(m_eventsList, 1);

    // Buttons in vertical layout
    eventsLayout->addWidget(m_removeEventButton);
    eventsLayout->addWidget(m_clearEventsButton);

    eventsWidget->setLayout(eventsLayout);
    m_eventsDock->setWidget(eventsWidget);
    m_eventsDock->setMinimumWidth(350);
    m_eventsDock->setVisible(false);  // Hidden by default
    addDockWidget(Qt::LeftDockWidgetArea, m_eventsDock);

    // Settings Dock
    m_settingsDock = new QDockWidget("Settings", this);
    m_settingsDock->setObjectName("SettingsDock");
    m_settingsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto* settingsWidget = new QWidget(this);
    auto* settingsLayout = new QVBoxLayout(settingsWidget);
    settingsLayout->setContentsMargins(8, 8, 8, 8);
    settingsLayout->setSpacing(8);

    // Speed section
    auto* speedLayout = new QHBoxLayout();
    speedLayout->addWidget(m_speedTextLabel);
    speedLayout->addWidget(m_speedSlider);
    speedLayout->addWidget(m_speedValueLabel);
    settingsLayout->addLayout(speedLayout);

    // Tracker section
    auto* trackerLayout = new QHBoxLayout();
    trackerLayout->addWidget(m_trackerTextLabel);
    trackerLayout->addWidget(m_trackerPickerButton);
    settingsLayout->addLayout(trackerLayout);

    // Load YOLO button
    settingsLayout->addWidget(m_loadYoloButton);

    // AI section
    auto* aiLayout = new QHBoxLayout();
    aiLayout->addWidget(m_aiEnabledCheckBox);
    aiLayout->addWidget(m_aiIntervalCombo);
    settingsLayout->addLayout(aiLayout);

    // YOLO status
    settingsLayout->addWidget(m_yoloStatusLabel);

    // Resolution section
    auto* resLayout = new QHBoxLayout();
    resLayout->addWidget(m_resolutionTextLabel);
    resLayout->addWidget(m_resolutionCombo);
    settingsLayout->addLayout(resLayout);

    settingsLayout->addStretch();
    settingsWidget->setLayout(settingsLayout);
    m_settingsDock->setWidget(settingsWidget);
    m_settingsDock->setMinimumWidth(300);
    m_settingsDock->setVisible(false);  // Hidden by default
    addDockWidget(Qt::RightDockWidgetArea, m_settingsDock);
}

void MainWindow::setupPlaybackToolbar() {
    m_playbackToolbar = addToolBar("Playback");
    m_playbackToolbar->setObjectName("PlaybackToolbar");
    m_playbackToolbar->setMovable(false);
    m_playbackToolbar->setFloatable(false);

    m_playbackToolbar->addWidget(m_seekBar);

    // Add spacer
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_playbackToolbar->addWidget(spacer);

    // Move toolbar to bottom
    addToolBar(Qt::BottomToolBarArea, m_playbackToolbar);
}

void MainWindow::createConnections() {
    // SeekBar connections
    connect(m_seekBar, &SeekBar::playPauseClicked, this, &MainWindow::onPlayPauseClicked);
    connect(m_seekBar, &SeekBar::sliderReleased, this, &MainWindow::onSeekReleased);
    connect(m_seekBar, &SeekBar::valueChanged, this, &MainWindow::onSeekValueChanged);
    
    // Other connections
    connect(m_resolutionCombo, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::onCameraResolutionChanged);
    connect(m_speedSlider, &QSlider::valueChanged, this, &MainWindow::onSpeedChanged);
    connect(m_removeEventButton, &QPushButton::clicked, this, &MainWindow::onRemoveEventClicked);
    connect(m_clearEventsButton, &QPushButton::clicked, this, &MainWindow::onClearEventsClicked);
    connect(m_eventsList, &QListWidget::itemDoubleClicked, this, &MainWindow::onEventActivated);
    connect(m_loadYoloButton, &QPushButton::clicked, this, &MainWindow::onLoadYoloModelClicked);
    connect(m_aiEnabledCheckBox, &QCheckBox::toggled, this, &MainWindow::onAiEnabledChanged);
    connect(m_aiIntervalCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onAiIntervalChanged);
}

void MainWindow::applyStylesheet() {
    setStyleSheet(
        "QWidget {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #070d15, stop:1 #0f1822);"
        "  color: #dfeaf5;"
        "}"
        "QMenuBar {"
        "  background: rgba(9, 15, 24, 0.92);"
        "  border-bottom: 1px solid rgba(168, 198, 226, 0.28);"
        "  color: #dfeaf5;"
        "}"
        "QMenuBar::item:hover {"
        "  background: rgba(56, 83, 111, 0.92);"
        "}"
        "QMenuBar::item:selected {"
        "  background: rgba(36, 57, 78, 0.98);"
        "  color: #ffffff;"
        "}"
        "QMenuBar::item:pressed {"
        "  background: rgba(36, 57, 78, 0.98);"
        "  color: #ffffff;"
        "}"
        "QMenu {"
        "  background: rgba(15, 27, 39, 0.96);"
        "  color: #f2f7ff;"
        "  border: 1px solid rgba(174, 205, 232, 0.45);"
        "}"
        "QMenu::item:selected {"
        "  background: rgba(54, 88, 116, 0.98);"
        "  color: #ffffff;"
        "}"
        "QMenu::item:hover {"
        "  background: rgba(68, 107, 138, 0.88);"
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
        "QDockWidget {"
        "  background: rgba(9, 15, 24, 0.82);"
        "  border: 1px solid rgba(168, 198, 226, 0.28);"
        "  border-radius: 6px;"
        "}"
        "QToolBar {"
        "  background: rgba(9, 15, 24, 0.82);"
        "  border: 1px solid rgba(168, 198, 226, 0.28);"
        "  border-radius: 6px;"
        "  spacing: 6px;"
        "  padding: 4px;"
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
        "SeekBar {"
        "  background: rgba(8, 14, 22, 0.92);"
        "  border: 1px solid rgba(168, 198, 226, 0.22);"
        "  border-radius: 8px;"
        "  padding: 6px 8px;"
        "}"
        "SeekBar QPushButton {"
        "  min-width: 86px;"
        "  max-width: 120px;"
        "  min-height: 30px;"
        "  padding: 4px 8px;"
        "}"
        "SeekBar QLabel {"
        "  color: #d7e8f7;"
        "  min-width: 48px;"
        "  padding: 0 2px;"
        "  font-family: Consolas, 'Courier New', monospace;"
        "  font-size: 9.5pt;"
        "}"
        "QLabel#seekTimeCurrent, QLabel#seekTimeTotal {"
        "  color: #b8d3e8;"
        "}"
        "SeekBar QSlider::groove:horizontal {"
        "  border: none;"
        "  height: 12px;"
        "  border-radius: 6px;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 rgba(70, 100, 120, 0.6),"
        "    stop:0.5 rgba(84, 112, 136, 0.5),"
        "    stop:1 rgba(70, 100, 120, 0.6));"
        "}"
        "SeekBar QSlider::sub-page:horizontal {"
        "  border-radius: 6px;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #5ab9ff,"
        "    stop:0.5 #3aacf0,"
        "    stop:1 #2a9fe8);"
        "}"
        "SeekBar QSlider::handle:horizontal {"
        "  width: 18px;"
        "  margin: -4px 0;"
        "  border-radius: 9px;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #ffffff,"
        "    stop:1 #e8f2ff);"
        "  border: 1px solid rgba(155, 199, 227, 0.8);"
        "  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.3);"
        "}"
        "SeekBar QSlider::handle:horizontal:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #ffffff,"
        "    stop:1 #f0f7ff);"
        "  border: 1px solid rgba(120, 180, 230, 1);"
        "  width: 20px;"
        "  margin: -5px 0;"
        "}"
    );
}

void MainWindow::restoreWindowState() {
    QSettings settings("DroneTracker", "DroneTracker");
    restoreGeometry(settings.value("geometry", saveGeometry()).toByteArray());
    restoreState(settings.value("windowState", saveState()).toByteArray());
}

void MainWindow::setPlaybackToolbarVisible(bool visible) {
    if (m_playbackToolbar) m_playbackToolbar->setVisible(visible);
}

void MainWindow::onSeekSliderHover(int frameIndex) {
    // Tooltip is now handled by SeekBar widget internally
    if (m_seekBar) {
        m_seekBar->setMetadata(m_totalVideoFrames, m_videoFps);
    }
}

MainWindow::~MainWindow() {
    stopCameraThread();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Save window state before closing
    QSettings settings("DroneTracker", "DroneTracker");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    
    stopCameraThread();
    QMainWindow::closeEvent(event);
}

// Menu action implementations
void MainWindow::onMenuOpenVideo() {
    onOpenVideoClicked();
}

void MainWindow::onMenuOpenCamera() {
    onCameraClicked();
}

void MainWindow::onMenuExit() {
    close();
}

void MainWindow::onMenuPlayPause() {
    onPlayPauseClicked();
}

void MainWindow::onMenuStop() {
    if (m_camera) {
        m_camera->setPaused(true);
        m_isPaused = true;
        m_seekBar->setValue(0);
        m_seekBar->setPlayButtonText("Play");
    }
}

void MainWindow::onMenuToggleEvents() {
    if (m_eventsDock) {
        m_eventsDock->setVisible(m_actionToggleEvents->isChecked());
    }
}

void MainWindow::onMenuToggleSettings() {
    if (m_settingsDock) {
        m_settingsDock->setVisible(m_actionToggleSettings->isChecked());
    }
}

void MainWindow::onMenuToggleDebug() {
    if (m_debugOverlay) {
        m_debugOverlay->toggleDebugOverlay();
        if (m_camera) {
            m_camera->setDebugEnabled(m_debugOverlay->isEnabled());
        }
    }
}

void MainWindow::onMenuFullscreen() {
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::onMenuLoadYolo() {
    onLoadYoloModelClicked();
}

void MainWindow::onMenuTrackerSettings() {
    // Show the tracker menu attached to the tracker button
    if (m_trackerPickerButton && m_trackerPickerButton->menu()) {
        m_trackerPickerButton->showMenu();
    }
}

void MainWindow::onMenuExportVideo() {
    onExportVideoClicked();
}

void MainWindow::onMenuExportYolo() {
    onExportYoloClicked();
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (event) {
        switch (event->key()) {
        case Qt::Key_F3:
            if (m_debugOverlay) {
                m_debugOverlay->toggleDebugOverlay();
                if (m_camera) {
                    m_camera->setDebugEnabled(m_debugOverlay->isEnabled());
                }
                m_actionToggleDebug->setChecked(m_debugOverlay->isEnabled());
            }
            event->accept();
            return;
        case Qt::Key_Space:
            if (!event->isAutoRepeat()) {
                onMenuPlayPause();
                event->accept();
                return;
            }
            break;
        case Qt::Key_Left:
            if (m_isVideoMode && m_seekBar) {
                int newValue = m_seekBar->value() - (event->modifiers() & Qt::ControlModifier ? 30 : 5);
                m_seekBar->setValue(qMax(0, newValue));
                event->accept();
                return;
            }
            break;
        case Qt::Key_Right:
            if (m_isVideoMode && m_seekBar) {
                int newValue = m_seekBar->value() + (event->modifiers() & Qt::ControlModifier ? 30 : 5);
                m_seekBar->setValue(qMin(m_totalVideoFrames, newValue));
                event->accept();
                return;
            }
            break;
        case Qt::Key_PageUp:
            if (m_isVideoMode && m_seekBar && m_videoFps > 0) {
                int framesToSeek = static_cast<int>(m_videoFps);  // 1 second
                int newValue = m_seekBar->value() - framesToSeek;
                m_seekBar->setValue(qMax(0, newValue));
                event->accept();
                return;
            }
            break;
        case Qt::Key_PageDown:
            if (m_isVideoMode && m_seekBar && m_videoFps > 0) {
                int framesToSeek = static_cast<int>(m_videoFps);  // 1 second
                int newValue = m_seekBar->value() + framesToSeek;
                m_seekBar->setValue(qMin(m_totalVideoFrames, newValue));
                event->accept();
                return;
            }
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            if (m_speedSlider) {
                int newValue = m_speedSlider->value() + 25;  // 0.25x increment
                m_speedSlider->setValue(qMin(300, newValue));
                event->accept();
                return;
            }
            break;
        case Qt::Key_Minus:
            if (m_speedSlider) {
                int newValue = m_speedSlider->value() - 25;  // 0.25x decrement
                m_speedSlider->setValue(qMax(25, newValue));
                event->accept();
                return;
            }
            break;
        case Qt::Key_1:
            if (m_speedSlider) {
                m_speedSlider->setValue(100);  // 1.0x
                event->accept();
                return;
            }
            break;
        case Qt::Key_L:
            onMenuLoadYolo();
            event->accept();
            return;
        case Qt::Key_T:
            onMenuTrackerSettings();
            event->accept();
            return;
        case Qt::Key_A:
            if (m_aiEnabledCheckBox) {
                m_aiEnabledCheckBox->toggle();
                event->accept();
                return;
            }
            break;
        default:
            break;
        }
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

    VideoExportEngine::ExportSettings settings;
    settings.sourcePath = m_currentVideoPath;
    settings.outputPath = outputPath;
    settings.trackerTypes = selectedTrackerTypes();
    settings.aiAssistEnabled = (m_aiEnabledCheckBox && m_aiEnabledCheckBox->isChecked() && !m_yoloModelPath.isEmpty());
    settings.yoloModelPath = m_yoloModelPath;
    settings.yoloClassNames = m_yoloClassNames;
    settings.aiIntervalFrames = std::max(1, m_aiIntervalCombo ? m_aiIntervalCombo->currentData().toInt() : 30);

    QProgressDialog progress("Exporting tracked video...", "Cancel", 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    VideoExportEngine engine;
    if (engine.export_video(settings, m_timeline, &progress)) {
        QMessageBox::information(this, "Export Video", "Export complete:\n" + outputPath);
    } else {
        QString errMsg = engine.lastError();
        if (errMsg.isEmpty()) {
            errMsg = "Export canceled.";
        }
        QMessageBox::warning(this, "Export Video", "Export failed: " + errMsg);
    }
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

    YoloExportEngine::ExportSettings settings;
    settings.sourcePath = m_currentVideoPath;
    settings.outputDirectory = outDir;
    settings.trackerTypes = selectedTrackerTypes();
    settings.aiAssistEnabled = (m_aiEnabledCheckBox && m_aiEnabledCheckBox->isChecked() && !m_yoloModelPath.isEmpty());
    settings.yoloModelPath = m_yoloModelPath;
    settings.yoloClassNames = m_yoloClassNames;
    settings.aiIntervalFrames = std::max(1, m_aiIntervalCombo ? m_aiIntervalCombo->currentData().toInt() : 30);

    QProgressDialog progress("Exporting YOLO labels...", "Cancel", 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    YoloExportEngine engine;
    if (engine.export_yolo(settings, m_timeline, &progress)) {
        const QString baseDir = outDir + "/" + inputInfo.completeBaseName() + "_yolo";
        QMessageBox::information(this, "Export YOLO", "YOLO data exported to:\n" + baseDir);
    } else {
        QString errMsg = engine.lastError();
        if (errMsg.isEmpty()) {
            errMsg = "Export canceled.";
        }
        QMessageBox::warning(this, "Export YOLO", "Export failed: " + errMsg);
    }
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

    // Enable AI controls after model is loaded
    if (m_aiEnabledCheckBox) m_aiEnabledCheckBox->setEnabled(true);
    if (m_aiIntervalCombo) m_aiIntervalCombo->setEnabled(true);

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

    const bool atVideoEnd = m_seekBar && (m_seekBar->maximum() > 0) && (m_seekBar->value() >= m_seekBar->maximum());
    if (m_isPaused && atVideoEnd) {
        m_camera->seekToFrame(0);
    }

    m_isPaused = !m_isPaused;
    m_seekBar->setPlayButtonText(m_isPaused ? "Play" : "Pause");
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

    const int frameIndex = m_seekBar->value();
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
    m_seekBar->setRange(0, totalFrames > 0 ? totalFrames - 1 : 0);
    m_seekBar->setMetadata(totalFrames, fps);
    updateSeekTimeLabel(m_seekBar->value());
}

void MainWindow::onVideoPosition(int frameIndex) {
    if (!m_seekBar->isSeeking()) {
        m_seekBar->setValue(frameIndex);
    }

    applyTrackingEventForFrame(frameIndex);
}

void MainWindow::onPlaybackEnded() {
    if (!m_isVideoMode) {
        return;
    }

    m_isPaused = true;
    if (m_seekBar) {
        m_seekBar->setPlayButtonText("Play");
    }
}

void MainWindow::onTrackerSelection(int x, int y, int width, int height) {
    if (m_isVideoMode) {
        const int frameIndex = currentVideoFrame();
        if (frameIndex >= 0) {
            TrackingEvent event;
            event.type = TrackingEventType::SetRoi;
            event.roi = QRect(x, y, width, height);
            m_timeline->addEvent(frameIndex, event);
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
            m_timeline->addEvent(frameIndex, event);
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

    m_timeline->clearAll();
    m_lastAppliedTimelineFrame = -1;
    m_totalVideoFrames = 0;
    m_videoFps = 30.0;
    refreshTrackingEventsUi();
    updateSeekTimeLabel(0);
    m_isVideoMode = !useGstreamer;
    m_currentVideoPath = useGstreamer ? QString() : source;
    m_isPaused = false;
    setVideoControlsEnabled(m_isVideoMode);
    setPlaybackToolbarVisible(m_isVideoMode);  // Hide seek bar in camera mode
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
    if (m_seekBar) {
        m_seekBar->setPlayButtonText("Pause");
    }

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

    if (m_debugOverlay && m_camera) {
        m_camera->setDebugEnabled(m_debugOverlay->isEnabled());
    }

    m_thread->start();

    if (m_isVideoMode) {
        onSpeedChanged(m_speedSlider->value());
    }

    if (m_seekBar) {
        m_seekBar->setPlayButtonText(m_isPaused ? "Play" : "Pause");
        m_seekBar->setMetadata(m_totalVideoFrames, m_videoFps);
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

    if (m_debugOverlay && !m_debugOverlay->isEnabled()) {
        m_debugOverlay->clear();
    }
}

void MainWindow::setVideoControlsEnabled(bool enabled) {
    if (m_seekBar) {
        m_seekBar->setEnabled(enabled);
        if (!enabled) {
            m_seekBar->setRange(0, 0);
            m_seekBar->setValue(0);
        }
    }
    if (m_speedSlider) {
        m_speedSlider->setEnabled(enabled);
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

    if (m_debugOverlay) {
        QTextEdit* overlayWidget = m_debugOverlay->widget();
        if (overlayWidget) {
            const QRect area = m_videoLabel->contentsRect();
            const int overlayWidth = std::min(520, std::max(260, area.width() / 2));
            const int overlayHeight = std::min(320, std::max(120, area.height() / 3));
            overlayWidget->setGeometry(area.left() + 12, area.top() + 12, overlayWidth, overlayHeight);
            overlayWidget->setVisible(m_debugOverlay->isEnabled() && m_debugOverlay->hasContent());
            overlayWidget->raise();
        }
    }
}

void MainWindow::onDebugInfoReady(const QString& info) {
    if (m_debugOverlay) {
        m_debugOverlay->updateDebugInfo(info);
    }
}

int MainWindow::currentVideoFrame() const {
    if (!m_isVideoMode || !m_seekBar) {
        return -1;
    }

    return m_seekBar->value();
}

void MainWindow::updateSeekTimeLabel(int currentFrame) {
    const int safeCurrentFrame = std::max(0, currentFrame);
    const int totalFrameForTime = std::max(0, m_totalVideoFrames);
    if (m_seekBar) {
        m_seekBar->setMetadata(totalFrameForTime, m_videoFps);
    }
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

    const TrackingEvent* evt = m_timeline->event(frameIndex);
    if (evt == nullptr) {
        m_lastAppliedTimelineFrame = frameIndex;
        return;
    }

    const TrackingEvent& event = *evt;
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
    m_timeline->removeEvent(frameIndex);
    m_lastAppliedTimelineFrame = -1;
    refreshTrackingEventsUi();
}

void MainWindow::onClearEventsClicked() {
    if (m_timeline->allEvents().isEmpty()) {
        return;
    }

    m_timeline->clearAll();
    m_lastAppliedTimelineFrame = -1;
    refreshTrackingEventsUi();
}

void MainWindow::onEventActivated(QListWidgetItem* item) {
    if (!item || !m_isVideoMode || !m_camera || !m_seekBar) {
        return;
    }

    const int frameIndex = item->data(Qt::UserRole).toInt();
    m_seekBar->setValue(frameIndex);
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
    for (auto it = m_timeline->allEvents().constBegin(); it != m_timeline->allEvents().constEnd(); ++it) {
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
