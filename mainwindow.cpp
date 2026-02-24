#include "mainwindow.h"

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
    
    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    
    // Camera selection group
    m_cameraGroup = new QGroupBox("Webcam", this);
    QVBoxLayout *cameraLayout = new QVBoxLayout(m_cameraGroup);
    
    // Camera combo box
    QHBoxLayout *comboLayout = new QHBoxLayout();
    QLabel *cameraLabel = new QLabel("Select Camera:", this);
    m_cameraCombo = new QComboBox(this);
    comboLayout->addWidget(cameraLabel);
    comboLayout->addWidget(m_cameraCombo, 1);
    cameraLayout->addLayout(comboLayout);
    
    // Tracker selection list
    QHBoxLayout *trackerLayout = new QHBoxLayout();
    QLabel *trackerLabel = new QLabel("Select Tracker(s):", this);
    m_trackerList = new QListWidget(this);
    m_trackerList->setSelectionMode(QAbstractItemView::MultiSelection);
    m_trackerList->setMaximumHeight(120);
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
    m_stopTrackingBtn = new QPushButton("Stop Tracking", this);
    m_clearSelectionBtn = new QPushButton("Clear Selection", this);
    
    m_stopTrackingBtn->setEnabled(false);
    
    buttonLayout->addWidget(m_stopTrackingBtn);
    buttonLayout->addWidget(m_clearSelectionBtn);
    buttonLayout->addStretch();
    
    mainLayout->addLayout(buttonLayout);
    
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
    
    // Update trackers if active
    if (m_isTracking && !m_trackers.isEmpty())
    {
        int successCount = 0;
        m_trackingRects.clear();
        
        // Update all trackers
        for (int i = 0; i < m_trackers.size(); i++)
        {
            cv::Rect rect = m_trackingRects.size() > i ? m_trackingRects[i] : m_averagedRect;
            bool ok = m_trackers[i]->update(m_currentFrame, rect);
            
            if (!ok) continue;
            m_trackingRects.append(rect);
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
    
    // Convert rectangle coordinates
    cv::Rect initRect = convertQRectToCvRect(rect, m_videoLabel->size(), 
                                            QSize(m_currentFrame.cols, m_currentFrame.rows));
    
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
    
    // Use the smaller scale to maintain aspect ratio
    double scale = qMin(scaleX, scaleY);
    
    // Calculate the offset (centering)
    int offsetX = (labelSize.width() - frameSize.width() / scale) / 2;
    int offsetY = (labelSize.height() - frameSize.height() / scale) / 2;
    
    // Convert coordinates
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

