#include "seekbar.h"
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QToolTip>

SeekSlider::SeekSlider(QWidget* parent)
    : QSlider(Qt::Horizontal, parent) {
    setMouseTracking(true);
    setTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(28);
}

void SeekSlider::setMetadata(int totalFrames, double fps) {
    m_totalFrames = totalFrames;
    m_fps = fps;
}

void SeekSlider::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        const int newValue = valueFromPosition(event->pos());
        setSliderDown(true);
        setValue(newValue);
        updateHoverTooltip(event->globalPos(), newValue);
        event->accept();
        return;
    }

    QSlider::mousePressEvent(event);
}

void SeekSlider::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        const int newValue = valueFromPosition(event->pos());
        setValue(newValue);
        updateHoverTooltip(event->globalPos(), newValue);
        event->accept();
        return;
    }

    updateHoverTooltip(event->globalPos(), valueFromPosition(event->pos()));
    QSlider::mouseMoveEvent(event);
}

void SeekSlider::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        const int newValue = valueFromPosition(event->pos());
        setValue(newValue);
        setSliderDown(false);
        emit sliderReleased();
        updateHoverTooltip(event->globalPos(), newValue);
        event->accept();
        return;
    }

    QSlider::mouseReleaseEvent(event);
}

void SeekSlider::leaveEvent(QEvent* event) {
    QToolTip::hideText();
    QSlider::leaveEvent(event);
}

int SeekSlider::valueFromPosition(const QPoint& position) const {
    QStyleOptionSlider option;
    initStyleOption(&option);

    const QRect grooveRect = style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, this);
    const QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);

    const int sliderMin = grooveRect.x();
    const int sliderSpan = qMax(1, grooveRect.width() - handleRect.width());
    const int centeredX = position.x() - (handleRect.width() / 2);
    const int boundedX = qBound(sliderMin, centeredX, sliderMin + sliderSpan);

    return QStyle::sliderValueFromPosition(minimum(), maximum(), boundedX - sliderMin, sliderSpan, option.upsideDown);
}

void SeekSlider::updateHoverTooltip(const QPoint& globalPos, int frameIndex) {
    if (maximum() <= 0 || m_totalFrames <= 0) {
        QToolTip::hideText();
        return;
    }

    const int clampedFrame = qBound(minimum(), frameIndex, maximum());
    const QString tooltipText = QString("%1 / %2")
        .arg(frameToTimeText(clampedFrame))
        .arg(frameToTimeText(maximum()));

    QToolTip::showText(globalPos + QPoint(0, -44), tooltipText, this);
}

QString SeekSlider::frameToTimeText(int frameIndex) const {
    if (m_fps <= 0.0) {
        return QString("00:00");
    }

    const int totalSeconds = static_cast<int>(frameIndex / m_fps);
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

SeekBar::SeekBar(QWidget* parent)
    : QWidget(parent) {
    
    // Create layout
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(12);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(42);

    // Create play/pause button
    m_playButton = new QPushButton("Play", this);
    m_playButton->setMinimumWidth(82);
    m_playButton->setMaximumWidth(96);
    m_playButton->setCursor(Qt::PointingHandCursor);
    connect(m_playButton, &QPushButton::clicked, this, &SeekBar::onPlayPauseClicked);

    m_currentTimeLabel = new QLabel("00:00", this);
    m_currentTimeLabel->setObjectName("seekTimeCurrent");
    m_currentTimeLabel->setMinimumWidth(52);
    m_currentTimeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_totalTimeLabel = new QLabel("00:00", this);
    m_totalTimeLabel->setObjectName("seekTimeTotal");
    m_totalTimeLabel->setMinimumWidth(52);
    m_totalTimeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    // Create slider
    m_slider = new SeekSlider(this);
    m_slider->setRange(0, 0);
    connect(m_slider, &QSlider::valueChanged, this, &SeekBar::onSliderValueChanged);
    connect(m_slider, &QSlider::sliderReleased, this, &SeekBar::onSliderReleased);

    // Add widgets to layout
    m_layout->addWidget(m_playButton);
    m_layout->addWidget(m_currentTimeLabel);
    m_layout->addWidget(m_slider);
    m_layout->addWidget(m_totalTimeLabel);
    m_layout->setStretch(2, 1);

    setLayout(m_layout);
}

SeekBar::~SeekBar() = default;

void SeekBar::setRange(int min, int max) {
    m_slider->setRange(min, max);
    updateTimeLabels();
}

void SeekBar::setValue(int value) {
    m_slider->setValue(value);
    updateTimeLabels();
}

int SeekBar::value() const {
    return m_slider->value();
}

int SeekBar::maximum() const {
    return m_slider->maximum();
}

void SeekBar::setPlayButtonText(const QString& text) {
    m_playButton->setText(text);
}

QString SeekBar::playButtonText() const {
    return m_playButton->text();
}

void SeekBar::setMetadata(int totalFrames, double fps) {
    m_totalFrames = totalFrames;
    m_fps = fps;
    if (m_slider) {
        m_slider->setMetadata(totalFrames, fps);
    }
    updateTimeLabels();
}

void SeekBar::updateTimeLabels() {
    const int currentFrame = m_slider ? m_slider->value() : 0;
    const int totalFrame = m_slider ? m_slider->maximum() : 0;

    auto frameToTime = [](int frameIndex, double fps) -> QString {
        if (fps <= 0.0) {
            return QString("00:00");
        }

        const int totalSeconds = static_cast<int>(frameIndex / fps);
        const int minutes = totalSeconds / 60;
        const int seconds = totalSeconds % 60;
        return QString("%1:%2")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    };

    const double fps = (m_fps > 0.0) ? m_fps : 30.0;
    const int displayTotalFrame = (m_totalFrames > 0) ? m_totalFrames : totalFrame;
    m_currentTimeLabel->setText(frameToTime(currentFrame, fps));
    m_totalTimeLabel->setText(frameToTime(displayTotalFrame, fps));
}

void SeekBar::onPlayPauseClicked() {
    emit playPauseClicked();
}

void SeekBar::onSliderValueChanged(int value) {
    updateTimeLabels();
    emit valueChanged(value);
}

void SeekBar::onSliderReleased() {
    emit sliderReleased();
}
