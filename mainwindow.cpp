#include "mainwindow.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_timer(new QTimer(this))
    , m_isTracking(false)
    , m_lastProcessingTime(0)
{
    setupUI();
    populateCameras();
    populateTrackers();
    
    m_frameTimer.start();
    
    connect(m_timer, &QTimer::timeout, this, &MainWindow::onUpdateFrame);
}

MainWindow::~MainWindow()
{
    if (m_capture.isOpened())
        m_capture.release();
}

void MainWindow::setupUI()
{
    // Create central widget
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    centralWidget->setStyleSheet(
        "QLabel { color: #d7e3ef; }"
        "QGroupBox {"
        "  color: #d7e3ef;"
        "  border: 1px solid #2a3d52;"
        "  border-radius: 10px;"
        "  margin-top: 12px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 10px;"
        "  padding: 0 6px;"
        "  color: #d7e3ef;"
        "}"
        "QComboBox {"
        "  background: #1f3042;"
        "  color: #e6eef7;"
        "  border: 1px solid #35506a;"
        "  border-radius: 10px;"
        "  padding: 6px 12px;"
        "  min-height: 24px;"
        "}"
        "QComboBox:hover {"
        "  border: 1px solid #4a6d8f;"
        "  background: #24374b;"
        "}"
        "QComboBox:focus {"
        "  border: 1px solid #58a6ff;"
        "}"
        "QComboBox::drop-down {"
        "  subcontrol-origin: padding;"
        "  subcontrol-position: top right;"
        "  width: 28px;"
        "  border-left: 1px solid #35506a;"
        "}"
        "QComboBox::down-arrow {"
        "  image: none;"
        "  width: 0;"
        "  height: 0;"
        "  border-left: 5px solid transparent;"
        "  border-right: 5px solid transparent;"
        "  border-top: 6px solid #9ec6ee;"
        "  margin-right: 8px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: #182636;"
        "  color: #d7e3ef;"
        "  border: 1px solid #35506a;"
        "  border-radius: 8px;"
        "  selection-background-color: #2d5d87;"
        "  selection-color: #ffffff;"
        "  padding: 4px;"
        "}"
        "QListWidget {"
        "  background: #182636;"
        "  color: #d7e3ef;"
        "  border: 1px solid #35506a;"
        "  border-radius: 10px;"
        "  padding: 6px;"
        "  outline: none;"
        "}"
        "QListWidget::item {"
        "  background: #1f3042;"
        "  border: 1px solid transparent;"
        "  border-radius: 8px;"
        "  padding: 6px 10px;"
        "  margin: 2px 0px;"
        "}"
        "QListWidget::item:hover {"
        "  background: #2a4057;"
        "  border: 1px solid #4a6d8f;"
        "}"
        "QListWidget::item:selected {"
        "  background: #2d5d87;"
        "  border: 1px solid #58a6ff;"
        "  color: #ffffff;"
        "}"
    );
    
    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    
    // Camera selection group
    m_cameraGroup = new QGroupBox("Webcam", this);
    QVBoxLayout *cameraLayout = new QVBoxLayout(m_cameraGroup);
    
    // Camera combo box
    QHBoxLayout *comboLayout = new QHBoxLayout();
    QLabel *cameraLabel = new QLabel("Select Camera:", this);
    m_cameraCombo = new QComboBox(this);
    m_cameraCombo->setMinimumHeight(36);
    comboLayout->addWidget(cameraLabel);
    comboLayout->addWidget(m_cameraCombo, 1);
    cameraLayout->addLayout(comboLayout);
    
    // Tracker selection list
    QHBoxLayout *trackerLayout = new QHBoxLayout();
    QLabel *trackerLabel = new QLabel("Select Tracker(s):", this);
    m_trackerList = new QListWidget(this);
    m_trackerList->setSelectionMode(QAbstractItemView::MultiSelection);
    m_trackerList->setMaximumHeight(120);
    m_trackerList->setSpacing(2);
    trackerLayout->addWidget(trackerLabel);
    trackerLayout->addWidget(m_trackerList, 1);
    cameraLayout->addLayout(trackerLayout);
    
    // Video display
    m_videoLabel = new VideoLabel(this);
    m_videoLabel->setStyleSheet("QLabel { background-color: black; border: 2px solid gray; }");
    m_videoLabel->setMinimumSize(640, 480);
    cameraLayout->addWidget(m_videoLabel);
    
    mainLayout->addWidget(m_cameraGroup);
    
    // Control buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_stopTrackingBtn = new OutlineButton("Stop Tracking", this);
    m_clearSelectionBtn = new OutlineButton("Clear Selection", this);

    m_stopTrackingBtn->setEnabled(false);

    buttonLayout->addWidget(m_stopTrackingBtn);
    buttonLayout->addWidget(m_clearSelectionBtn);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);

    // AI Control Section
    QGroupBox *aiGroup = new QGroupBox("AI Object Recognition", this);
    QVBoxLayout *aiLayout = new QVBoxLayout(aiGroup);

    // AI buttons
    QHBoxLayout *aiButtonLayout = new QHBoxLayout();
    m_aiToggleBtn = new OutlineButton("Enable AI", this);
    m_aiToggleBtn->setCheckable(true);
    m_aiToggleBtn->setEnabled(false);
    m_aiSaveBtn = new OutlineButton("Save Model", this);
    m_aiLoadBtn = new OutlineButton("Load Model", this);
    OutlineButton *yoloBtn = new OutlineButton("Use YOLO", this);

    aiButtonLayout->addWidget(m_aiToggleBtn);
    aiButtonLayout->addWidget(m_aiSaveBtn);
    aiButtonLayout->addWidget(m_aiLoadBtn);
    aiButtonLayout->addWidget(yoloBtn);
    aiLayout->addLayout(aiButtonLayout);

    // AI settings
    QHBoxLayout *aiSettingsLayout = new QHBoxLayout();
    QLabel *intervalLabel = new QLabel("Detect every:", this);
    m_aiFrameIntervalCombo = new QComboBox(this);
    m_aiFrameIntervalCombo->addItem("5 frames", 5);
    m_aiFrameIntervalCombo->addItem("10 frames", 10);
    m_aiFrameIntervalCombo->addItem("15 frames", 15);
    m_aiFrameIntervalCombo->addItem("20 frames", 20);
    m_aiFrameIntervalCombo->addItem("30 frames", 30);
    m_aiFrameIntervalCombo->setCurrentIndex(2); // 15 frames by default
    m_aiFrameIntervalCombo->setEnabled(false);
    aiSettingsLayout->addWidget(intervalLabel);
    aiSettingsLayout->addWidget(m_aiFrameIntervalCombo);
    aiSettingsLayout->addStretch();
    aiLayout->addLayout(aiSettingsLayout);

    // AI status
    m_aiStatusLabel = new QLabel("AI: Disabled", this);
    m_aiStatusLabel->setStyleSheet("color: #888;");
    aiLayout->addWidget(m_aiStatusLabel);

    mainLayout->addWidget(aiGroup);

    // Connect AI signals
    connect(m_aiToggleBtn, &QPushButton::clicked, this, &MainWindow::onToggleAI);
    connect(m_aiSaveBtn, &QPushButton::clicked, this, &MainWindow::onSaveAIModel);
    connect(m_aiLoadBtn, &QPushButton::clicked, this, &MainWindow::onLoadAIModel);
    connect(m_aiFrameIntervalCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onAIFrameIntervalChanged);
    connect(yoloBtn, &QPushButton::clicked, this, &MainWindow::onSetupYOLO);
    
    // Status and performance info
    m_statusLabel = new QLabel("Status: No camera selected", this);
    mainLayout->addWidget(m_statusLabel);
    
    QHBoxLayout *metricsLayout = new QHBoxLayout();
    m_fpsLabel = new QLabel("FPS: 0.0", this);
    m_delayLabel = new QLabel("Delay: 0 ms", this);
    metricsLayout->addWidget(m_fpsLabel);
    metricsLayout->addWidget(m_delayLabel);
    metricsLayout->addStretch();
    mainLayout->addLayout(metricsLayout);
    
    // Connect signals
    connect(m_cameraCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MainWindow::onCameraChanged);
    connect(m_trackerList, &QListWidget::itemSelectionChanged,
            this, &MainWindow::onTrackerSelectionChanged);
    connect(m_videoLabel, &VideoLabel::rectangleSelected, 
            this, &MainWindow::onRectangleSelected);
    connect(m_stopTrackingBtn, &QPushButton::clicked, 
            this, &MainWindow::onStopTracking);
    connect(m_clearSelectionBtn, &QPushButton::clicked, 
            this, &MainWindow::onClearSelection);
    
    // Set window properties
    setWindowTitle("Computer Vision - Webcam Tracker");
    resize(800, 700);
}

void MainWindow::populateCameras()
{
    m_cameraCombo->addItem("Select a camera...", -1);
    
    // Try to detect available cameras (usually 0-5 are common indices)
    for (int i = 0; i < 10; i++)
    {
        cv::VideoCapture testCapture(i);
        if (testCapture.isOpened())
        {
            m_cameraCombo->addItem(QString("Camera %1").arg(i), i);
            testCapture.release();
        }
    }
}

void MainWindow::populateTrackers()
{
    QListWidgetItem *item;
    
    item = new QListWidgetItem("MIL", m_trackerList);
    item->setData(Qt::UserRole, TRACKER_MIL);
    
    item = new QListWidgetItem("KCF", m_trackerList);
    item->setData(Qt::UserRole, TRACKER_KCF);
    
    item = new QListWidgetItem("CSRT (Recommended)", m_trackerList);
    item->setData(Qt::UserRole, TRACKER_CSRT);
    
    // Initially disable drawing since no tracker is selected
    m_videoLabel->setEnabled(false);
}

void MainWindow::onCameraChanged(int index)
{
    // Stop current capture
    if (m_capture.isOpened())
    {
        m_timer->stop();
        m_capture.release();
    }
    
    onStopTracking();
    m_videoLabel->clearSelection();
    
    if (index <= 0)
    {
        m_statusLabel->setText("Status: No camera selected");
        return;
    }
    
    int cameraIndex = m_cameraCombo->itemData(index).toInt();
    
    if (m_capture.open(cameraIndex))
    {
        m_statusLabel->setText(QString("Status: Camera %1 active").arg(cameraIndex));
        m_timer->start(33); // ~30 FPS
    }
    else
    {
        m_statusLabel->setText(QString("Status: Failed to open Camera %1").arg(cameraIndex));
    }
}

void MainWindow::onUpdateFrame()
{
    qint64 frameStartTime = m_frameTimer.elapsed();

    if (!m_capture.isOpened())
        return;

    m_capture >> m_currentFrame;

    if (m_currentFrame.empty())
        return;

    cv::Mat displayFrame = m_currentFrame.clone();

    // AI Detection every N frames
    if (m_aiEnabled && m_aiCore.hasTarget())
    {
        m_frameCounter++;
        if (m_frameCounter >= m_aiFrameInterval)
        {
            m_frameCounter = 0;
            runAIDetection(displayFrame);
        }
    }

    // Update trackers if active
    if (m_isTracking && !m_trackers.isEmpty())
    {
        int successCount = 0;
        m_trackingRects.clear();

        // Update all trackers
        for (int i = 0; i < m_trackers.size(); i++)
        {
            cv::Rect updatedRect;
            bool ok = m_trackers[i]->update(m_currentFrame, updatedRect);

            if (!ok) continue;
            m_trackingRects.append(updatedRect);
            successCount++;
        }

        if (successCount > 0)
        {
            // Calculate averaged rectangle
            int avgX = 0, avgY = 0, avgW = 0, avgH = 0;
            for (const cv::Rect &rect : m_trackingRects)
            {
                avgX += rect.x;
                avgY += rect.y;
                avgW += rect.width;
                avgH += rect.height;
            }
            avgX /= successCount;
            avgY /= successCount;
            avgW /= successCount;
            avgH /= successCount;

            m_averagedRect = cv::Rect(avgX, avgY, avgW, avgH);
            cv::rectangle(displayFrame, m_averagedRect, cv::Scalar(0, 255, 0), 2);

            // Draw center point
            cv::Point center(m_averagedRect.x + m_averagedRect.width / 2,
                           m_averagedRect.y + m_averagedRect.height / 2);
            cv::circle(displayFrame, center, 3, cv::Scalar(0, 0, 255), -1);

            QString status = QString("Status: Tracking (%1 tracker%2) - Position (%3, %4)")
                            .arg(successCount)
                            .arg(successCount > 1 ? "s" : "")
                            .arg(m_averagedRect.x).arg(m_averagedRect.y);
            m_statusLabel->setText(status);
        }
        else
        {
            m_statusLabel->setText("Status: All trackers lost!");
            onStopTracking();
        }
    }
    
    // Calculate processing time and update FPS
    qint64 frameEndTime = m_frameTimer.elapsed();
    m_lastProcessingTime = frameEndTime - frameStartTime;
    m_frameTimes.append(frameEndTime);
    
    // Keep only last 30 frames for FPS calculation
    if (m_frameTimes.size() > 30)
        m_frameTimes.removeFirst();
    
    updateFpsDisplay();
    
    m_videoLabel->setFrame(displayFrame);
}

void MainWindow::onRectangleSelected(QRect rect)
{
    if (!m_capture.isOpened() || m_currentFrame.empty())
        return;

    QList<TrackerType> selectedTrackers = getSelectedTrackers();
    if (selectedTrackers.isEmpty())
    {
        m_statusLabel->setText("Status: No tracker selected!");
        return;
    }

    // Clear the selection rectangle from the video label since tracking will show it
    m_videoLabel->clearSelection();

    // Get the actual scaled image size and offset from VideoLabel
    QSize scaledSize = m_videoLabel->getScaledImageSize();
    QPoint offset = m_videoLabel->getImageOffset();
    
    // Convert rectangle coordinates using the actual scaled image info
    cv::Rect initRect = convertQRectToCvRect(rect, m_videoLabel->size(),
                                            QSize(m_currentFrame.cols, m_currentFrame.rows),
                                            scaledSize, offset);

    m_averagedRect = initRect;

    // Initialize all selected trackers
    m_trackers.clear();
    m_trackingRects.clear();

    try
    {
        for (TrackerType type : selectedTrackers)
        {
            cv::Ptr<cv::Tracker> tracker = createTracker(type);
            tracker->init(m_currentFrame, initRect);
            m_trackers.append(tracker);
            m_trackingRects.append(initRect);
        }

        m_isTracking = true;
        m_stopTrackingBtn->setEnabled(true);

        // Learn the object for AI recognition
        if (m_aiCore.learnObject(m_currentFrame, initRect, "TrackedObject"))
        {
            m_aiToggleBtn->setEnabled(true);
            if (m_aiEnabled)
            {
                m_frameCounter = 0; // Reset counter when starting tracking
            }
        }

        QString status = QString("Status: Tracking started with %1 tracker%2")
                        .arg(m_trackers.size())
                        .arg(m_trackers.size() > 1 ? "s" : "");
        m_statusLabel->setText(status);
    }
    catch (...)
    {
        m_statusLabel->setText("Status: Failed to initialize trackers");
        m_trackers.clear();
        m_trackingRects.clear();
    }
}

void MainWindow::onStopTracking()
{
    if (!m_isTracking) return;

    m_isTracking = false;
    m_trackers.clear();
    m_trackingRects.clear();
    m_stopTrackingBtn->setEnabled(false);
    
    m_statusLabel->setText("Status: Tracking stopped");
}

void MainWindow::onClearSelection()
{
    m_videoLabel->clearSelection();
    onStopTracking();
    
    if (m_capture.isOpened())
    {
        m_statusLabel->setText("Status: Selection cleared");
    }
}

cv::Rect MainWindow::convertQRectToCvRect(const QRect &qrect, const QSize &labelSize, const QSize &frameSize)
{
    // Calculate scaling factors
    double scaleX = static_cast<double>(frameSize.width()) / labelSize.width();
    double scaleY = static_cast<double>(frameSize.height()) / labelSize.height();
    
    // Use the larger scale (image is scaled to fit while maintaining aspect ratio)
    // The image will be letterboxed/pillarboxed in the label
    double scale = qMax(scaleX, scaleY);
    
    // Calculate the actual scaled image size
    int scaledWidth = static_cast<int>(frameSize.width() / scale);
    int scaledHeight = static_cast<int>(frameSize.height() / scale);
    
    // Calculate the offset (centering) - where the image starts within the label
    int offsetX = (labelSize.width() - scaledWidth) / 2;
    int offsetY = (labelSize.height() - scaledHeight) / 2;
    
    // Convert coordinates from label space to frame space
    int x = static_cast<int>((qrect.x() - offsetX) * scale);
    int y = static_cast<int>((qrect.y() - offsetY) * scale);
    int w = static_cast<int>(qrect.width() * scale);
    int h = static_cast<int>(qrect.height() * scale);
    
    // Clamp to frame boundaries
    x = qMax(0, qMin(x, frameSize.width() - 1));
    y = qMax(0, qMin(y, frameSize.height() - 1));
    w = qMax(1, qMin(w, frameSize.width() - x));
    h = qMax(1, qMin(h, frameSize.height() - y));
    
    return cv::Rect(x, y, w, h);
}

cv::Rect MainWindow::convertQRectToCvRect(const QRect &qrect, const QSize &labelSize, const QSize &frameSize,
                                           const QSize &scaledSize, const QPoint &offset)
{
    // Calculate scale from original frame to scaled image displayed on screen
    double scaleX = static_cast<double>(scaledSize.width()) / frameSize.width();
    double scaleY = static_cast<double>(scaledSize.height()) / frameSize.height();
    
    // Both scales should be equal since Qt maintains aspect ratio
    double scale = qMin(scaleX, scaleY);
    
    // Convert coordinates from label space (widget coordinates) to frame space
    // First subtract the offset to get coordinates within the scaled image
    // Then divide by scale to get original frame coordinates
    int x = static_cast<int>((qrect.x() - offset.x()) / scale);
    int y = static_cast<int>((qrect.y() - offset.y()) / scale);
    int w = static_cast<int>(qrect.width() / scale);
    int h = static_cast<int>(qrect.height() / scale);
    
    // Clamp to frame boundaries
    x = qMax(0, qMin(x, frameSize.width() - 1));
    y = qMax(0, qMin(y, frameSize.height() - 1));
    w = qMax(1, qMin(w, frameSize.width() - x));
    h = qMax(1, qMin(h, frameSize.height() - y));
    
    return cv::Rect(x, y, w, h);
}

cv::Ptr<cv::Tracker> MainWindow::createTracker(TrackerType type)
{
    switch (type)
    {
        case TRACKER_MIL:
            return cv::TrackerMIL::create();
        case TRACKER_KCF:
            return cv::TrackerKCF::create();
        case TRACKER_CSRT:
            return cv::TrackerCSRT::create();
        default:
            return cv::TrackerCSRT::create();
    }
}

QList<TrackerType> MainWindow::getSelectedTrackers()
{
    QList<TrackerType> trackers;
    QList<QListWidgetItem*> selectedItems = m_trackerList->selectedItems();
    
    for (QListWidgetItem *item : selectedItems)
    {
        TrackerType type = static_cast<TrackerType>(item->data(Qt::UserRole).toInt());
        trackers.append(type);
    }
    
    return trackers;
}

void MainWindow::onTrackerSelectionChanged()
{
    updateDrawingEnabled();
}

void MainWindow::updateDrawingEnabled()
{
    bool hasSelection = !m_trackerList->selectedItems().isEmpty();
    m_videoLabel->setEnabled(hasSelection);
    
    if (!hasSelection)
    {
        m_statusLabel->setText("Status: Select at least one tracker to enable drawing");
    }
    else if (m_capture.isOpened() && !m_isTracking)
    {
        int count = m_trackerList->selectedItems().size();
        m_statusLabel->setText(QString("Status: %1 tracker%2 selected - Draw a rectangle to track")
                              .arg(count).arg(count > 1 ? "s" : ""));
    }
}

void MainWindow::updateFpsDisplay()
{
    if (m_frameTimes.size() >= 2)
    {
        qint64 timeDiff = m_frameTimes.last() - m_frameTimes.first();
        double fps = (m_frameTimes.size() - 1) * 1000.0 / timeDiff;
        m_fpsLabel->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
    }

    m_delayLabel->setText(QString("Delay: %1 ms").arg(m_lastProcessingTime));
}

void MainWindow::runAIDetection(cv::Mat &displayFrame)
{
    if (!m_aiEnabled || !m_aiCore.hasTarget() || m_currentFrame.empty())
        return;

    try
    {
        AIDetectionResult result = m_aiCore.findObject(m_currentFrame);

        if (result.detected)
        {
            // Convert QRect to cv::Rect
            cv::Rect aiRect(result.boundingRect.x(), result.boundingRect.y(),
                            result.boundingRect.width(), result.boundingRect.height());

            // Проверка на валидность прямоугольника
            if (aiRect.area() <= 0 || aiRect.width > m_currentFrame.cols || 
                aiRect.height > m_currentFrame.rows)
            {
                m_statusLabel->setText("Status: AI invalid result");
                return;
            }

            // Reinitialize trackers with AI position
            reinitTrackers(aiRect);

            // Draw AI detection on the frame
            cv::rectangle(displayFrame, aiRect, cv::Scalar(255, 0, 0), 2);
            cv::putText(displayFrame, "AI Corrected", cv::Point(aiRect.x, aiRect.y - 10),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 0, 0), 2);

            m_statusLabel->setText(QString("Status: AI corrected trackers - Confidence: %1%")
                                  .arg(static_cast<int>(result.confidence * 100)));
        }
        else
        {
            m_statusLabel->setText("Status: AI detection - Object not found");
        }
    }
    catch (const cv::Exception &e)
    {
        qWarning() << "AI detection error:" << e.what();
        m_statusLabel->setText("Status: AI error - continuing with trackers");
    }
    catch (...)
    {
        qWarning() << "AI detection: unknown error";
        m_statusLabel->setText("Status: AI error");
    }
}

void MainWindow::reinitTrackers(const cv::Rect &newRect)
{
    if (!m_capture.isOpened() || m_currentFrame.empty())
        return;

    // Проверка на валидность прямоугольника
    if (newRect.area() <= 0 || newRect.x < 0 || newRect.y < 0 ||
        newRect.x + newRect.width > m_currentFrame.cols ||
        newRect.y + newRect.height > m_currentFrame.rows)
    {
        return;
    }

    // Get currently selected tracker types
    QList<TrackerType> selectedTrackers = getSelectedTrackers();
    if (selectedTrackers.isEmpty())
        return;

    try
    {
        // Clear old trackers
        m_trackers.clear();
        m_trackingRects.clear();

        // Reinitialize all trackers with the new position
        for (TrackerType type : selectedTrackers)
        {
            cv::Ptr<cv::Tracker> tracker = createTracker(type);
            if (!tracker.empty())
            {
                tracker->init(m_currentFrame, newRect);
                m_trackers.append(tracker);
                m_trackingRects.append(newRect);
            }
        }

        if (!m_trackers.isEmpty())
            m_averagedRect = newRect;
    }
    catch (const cv::Exception &e)
    {
        qWarning() << "Tracker reinit error:" << e.what();
        m_trackers.clear();
        m_trackingRects.clear();
    }
    catch (...)
    {
        qWarning() << "Tracker reinit: unknown error";
        m_trackers.clear();
        m_trackingRects.clear();
    }
}

void MainWindow::onToggleAI()
{
    m_aiEnabled = !m_aiEnabled;
    m_frameCounter = 0;

    if (m_aiEnabled)
    {
        m_aiToggleBtn->setText("Disable AI");
        m_aiToggleBtn->setChecked(true);
        m_aiFrameIntervalCombo->setEnabled(true);

        if (m_aiCore.hasTarget())
        {
            m_aiStatusLabel->setText(QString("AI: Active (every %1 frames)").arg(m_aiFrameInterval));
            m_aiStatusLabel->setStyleSheet("color: #00ff00;");
        }
        else
        {
            m_aiStatusLabel->setText("AI: No trained object");
            m_aiStatusLabel->setStyleSheet("color: #ffaa00;");
        }
    }
    else
    {
        m_aiToggleBtn->setText("Enable AI");
        m_aiToggleBtn->setChecked(false);
        m_aiFrameIntervalCombo->setEnabled(false);
        m_aiStatusLabel->setText("AI: Disabled");
        m_aiStatusLabel->setStyleSheet("color: #888;");
    }
}

void MainWindow::onSaveAIModel()
{
    if (!m_aiCore.hasTarget())
    {
        m_aiStatusLabel->setText("AI: No object to save");
        return;
    }

    QString defaultPath = "models/trained_object.dat";
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Save AI Model",
        defaultPath,
        "AI Model Files (*.dat);;All Files (*)"
    );

    if (filePath.isEmpty())
        return;

    if (m_aiCore.saveTarget(filePath))
    {
        m_aiStatusLabel->setText("AI: Model saved successfully");
        m_statusLabel->setText("Status: AI model saved to " + QFileInfo(filePath).fileName());
    }
    else
    {
        m_aiStatusLabel->setText("AI: Failed to save model");
    }
}

void MainWindow::onLoadAIModel()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Load AI Model",
        "models/",
        "AI Model Files (*.dat);;All Files (*)"
    );

    if (filePath.isEmpty())
        return;

    if (m_aiCore.loadTarget(filePath))
    {
        m_aiStatusLabel->setText(QString("AI: Loaded '%1'").arg(m_aiCore.targetName()));
        m_aiStatusLabel->setStyleSheet("color: #00ff00;");
        m_aiToggleBtn->setEnabled(true);
        m_statusLabel->setText("Status: AI model loaded - ready to track");
    }
    else
    {
        m_aiStatusLabel->setText("AI: Failed to load model");
    }
}

void MainWindow::onAIFrameIntervalChanged(int index)
{
    int frames = m_aiFrameIntervalCombo->itemData(index).toInt();
    m_aiFrameInterval = frames;

    if (m_aiEnabled)
    {
        m_aiStatusLabel->setText(QString("AI: Active (every %1 frames)").arg(frames));
    }
}

void MainWindow::onSetupYOLO()
{
    // Диалог выбора модели
    QString modelPath = QFileDialog::getOpenFileName(
        this,
        "Select YOLO Model",
        "models/",
        "ONNX Models (*.onnx);;All Files (*)"
    );

    if (modelPath.isEmpty())
        return;

    // Диалог выбора класса
    QStringList cocoClasses = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat",
        "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat",
        "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack",
        "umbrella", "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball",
        "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket",
        "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
        "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake",
        "chair", "couch", "potted plant", "bed", "dining table", "toilet", "tv", "laptop",
        "mouse", "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink",
        "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"
    };

    bool ok = false;
    QString selectedClass = QInputDialog::getItem(
        this,
        "Select Object Class",
        "Choose the class to track:",
        cocoClasses,
        0, // person by default
        false,
        &ok
    );

    if (!ok || selectedClass.isEmpty())
        return;

    // Находим ID класса
    int classId = AICore::getCOCOClassId(selectedClass);

    // Настраиваем YOLO
    m_aiCore.setupYOLO(modelPath, selectedClass, classId);

    m_aiStatusLabel->setText(QString("YOLO: %1 (%2)").arg(selectedClass).arg(modelPath.split('/').last()));
    m_aiStatusLabel->setStyleSheet("color: #00ff00;");
    m_aiToggleBtn->setEnabled(true);

    m_statusLabel->setText(QString("Status: YOLO configured for '%1'").arg(selectedClass));
}

