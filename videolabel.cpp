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

    cv::Mat rgb;
    if (frame.channels() == 1)
        cv::cvtColor(frame, rgb, cv::COLOR_GRAY2RGB);
    else
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);

    QImage image(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
    m_currentPixmap = QPixmap::fromImage(image);
    
    update();
}

void VideoLabel::clearSelection()
{
    m_hasSelection = false;
    m_selectedRect = QRect();
    update();
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
    
    if (m_currentPixmap.isNull()) return;

    QPainter painter(this);
    
    // Calculate scaled pixmap to fit label while maintaining aspect ratio
    QPixmap scaled = m_currentPixmap.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    // Center the pixmap
    int x = (width() - scaled.width()) / 2;
    int y = (height() - scaled.height()) / 2;
    
    painter.drawPixmap(x, y, scaled);
    
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
