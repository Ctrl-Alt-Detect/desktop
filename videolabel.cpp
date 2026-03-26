#include "videolabel.h"
#include <QPainter>
#include <QImage>

VideoLabel::VideoLabel(QWidget *parent)
    : QLabel(parent)
    , m_drawing(false)
    , m_hasSelection(false)
{
    setMouseTracking(true);
    setScaledContents(false);
    setAlignment(Qt::AlignCenter);
    setMinimumSize(320, 240);
}

void VideoLabel::setFrame(const cv::Mat &frame)
{
    if (frame.empty()) return;

    if (frame.channels() == 1)
    {
        cv::cvtColor(frame, m_rgbMat, cv::COLOR_GRAY2RGB);
        m_frameImage = QImage(m_rgbMat.data,
                              m_rgbMat.cols,
                              m_rgbMat.rows,
                              static_cast<int>(m_rgbMat.step),
                              QImage::Format_RGB888);
    }
    else
    {
        m_rgbMat = frame;
        m_frameImage = QImage(m_rgbMat.data,
                              m_rgbMat.cols,
                              m_rgbMat.rows,
                              static_cast<int>(m_rgbMat.step),
                              QImage::Format_BGR888);
    }
    m_frameDirty = true;

    update();
}

void VideoLabel::clearSelection()
{
    m_hasSelection = false;
    m_selectedRect = QRect();
    update();
}

QSize VideoLabel::getScaledImageSize() const
{
    return m_scaledSize;
}

QPoint VideoLabel::getImageOffset() const
{
    int x = (width() - m_scaledImage.width()) / 2;
    int y = (height() - m_scaledImage.height()) / 2;
    return QPoint(x, y);
}

void VideoLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_startPoint = event->pos();
        m_endPoint = event->pos();
        m_drawing = true;
        m_hasSelection = false;
    }
}

void VideoLabel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drawing)
    {
        m_endPoint = event->pos();
        update();
    }
}

void VideoLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_drawing)
    {
        m_endPoint = event->pos();
        m_drawing = false;

        // Calculate the rectangle in widget coordinates
        int x = qMin(m_startPoint.x(), m_endPoint.x());
        int y = qMin(m_startPoint.y(), m_endPoint.y());
        int w = qAbs(m_endPoint.x() - m_startPoint.x());
        int h = qAbs(m_endPoint.y() - m_startPoint.y());

        if (w > 5 && h > 5) // Minimum size threshold
        {
            m_selectedRect = QRect(x, y, w, h);
            m_hasSelection = true;
            emit rectangleSelected(m_selectedRect);
        }

        update();
    }
}

void VideoLabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event);

    if (m_frameImage.isNull()) return;

    QPainter painter(this);

    // Calculate scaled pixmap to fit label while maintaining aspect ratio
    QSize targetSize = m_frameImage.size();
    targetSize.scale(size(), Qt::KeepAspectRatio);

    // Only rescale if frame changed OR if size changed significantly (more than 5 pixels)
    bool sizeChanged = (m_scaledSize != targetSize) && 
                       (qAbs(m_scaledSize.width() - targetSize.width()) > 5 || 
                        qAbs(m_scaledSize.height() - targetSize.height()) > 5);
    
    if (m_frameDirty || sizeChanged)
    {
        // Use FastTransformation for real-time video - much faster than SmoothTransformation
        m_scaledImage = m_frameImage.scaled(targetSize, Qt::KeepAspectRatio, Qt::FastTransformation);
        m_scaledSize = targetSize;
        m_frameDirty = false;
    }

    // Center the pixmap
    int x = (width() - m_scaledImage.width()) / 2;
    int y = (height() - m_scaledImage.height()) / 2;

    painter.drawImage(x, y, m_scaledImage);

    // Draw the selection rectangle
    if (m_drawing || m_hasSelection)
    {
        painter.setPen(QPen(Qt::green, 2));

        QRect drawRect;
        if (m_drawing)
        {
            int rx = qMin(m_startPoint.x(), m_endPoint.x());
            int ry = qMin(m_startPoint.y(), m_endPoint.y());
            int rw = qAbs(m_endPoint.x() - m_startPoint.x());
            int rh = qAbs(m_endPoint.y() - m_startPoint.y());
            drawRect = QRect(rx, ry, rw, rh);
        }
        else if (m_hasSelection)
        {
            drawRect = m_selectedRect;
        }

        painter.drawRect(drawRect);
    }
}
