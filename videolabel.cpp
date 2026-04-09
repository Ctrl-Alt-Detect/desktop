#include "videolabel.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <algorithm>

VideoLabel::VideoLabel(QWidget* parent)
    : QLabel(parent) {
    setAlignment(Qt::AlignCenter);
    setScaledContents(true);
}

void VideoLabel::setFrameSize(int width, int height) {
    m_frameWidth = width;
    m_frameHeight = height;
}

void VideoLabel::paintEvent(QPaintEvent* event) {
    QLabel::paintEvent(event);

    if (!m_selecting) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(0, 255, 0), 2, Qt::DashLine));
    painter.setBrush(QColor(0, 255, 0, 40));
    painter.drawRect(QRect(m_startPoint, m_currentPoint).normalized());
}

QPoint VideoLabel::mapToFrameCoords(const QPoint& widgetPos) const {
    if (m_frameWidth == 0 || m_frameHeight == 0) {
        return widgetPos;
    }

    const QRect contentRect = contentsRect();

    if (contentRect.width() <= 0 || contentRect.height() <= 0) {
        return widgetPos;
    }

    // Label uses scaled contents, so the image is stretched to fill contentsRect.
    const float relX = static_cast<float>(widgetPos.x() - contentRect.left()) / contentRect.width();
    const float relY = static_cast<float>(widgetPos.y() - contentRect.top()) / contentRect.height();

    const float clampedRelX = std::max(0.0f, std::min(1.0f, relX));
    const float clampedRelY = std::max(0.0f, std::min(1.0f, relY));

    const int frameX = std::max(0, std::min(m_frameWidth - 1, static_cast<int>(clampedRelX * m_frameWidth)));
    const int frameY = std::max(0, std::min(m_frameHeight - 1, static_cast<int>(clampedRelY * m_frameHeight)));

    return QPoint(frameX, frameY);
}

void VideoLabel::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_startPoint = event->pos();
        m_currentPoint = event->pos();
        m_selecting = true;
    } else if (event->button() == Qt::RightButton) {
        emit trackerReset();
    }
    QLabel::mousePressEvent(event);
}

void VideoLabel::mouseMoveEvent(QMouseEvent* event) {
    if (m_selecting) {
        m_currentPoint = event->pos();
        update();
    }
    QLabel::mouseMoveEvent(event);
}

void VideoLabel::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_currentPoint = event->pos();
        m_selecting = false;

        // Map widget coordinates to frame coordinates
        const QPoint startFrame = mapToFrameCoords(m_startPoint);
        const QPoint endFrame = mapToFrameCoords(m_currentPoint);
        
        const int x = std::min(startFrame.x(), endFrame.x());
        const int y = std::min(startFrame.y(), endFrame.y());
        const int width = std::abs(endFrame.x() - startFrame.x());
        const int height = std::abs(endFrame.y() - startFrame.y());

        if (width > 10 && height > 10) {
            emit selectionMade(x, y, width, height);
        }
        update();
    }
    QLabel::mouseReleaseEvent(event);
}
