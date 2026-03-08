#include "videolabel.h"
#include <QPainter>
#include <QImage>

VideoLabel::VideoLabel(QWidget *parent)
    : QLabel(parent)
    , m_drawing(false)
    , m_hasSelection(false)
    , m_frameDirty(true)
    , m_videoOffset(0, 0)
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

QRectF VideoLabel::getVideoAreaRect() const
{
    return QRectF(m_videoOffset, m_currentVideoSize);
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
        // Проверяем что клик внутри области видео
        QRectF videoRect = getVideoAreaRect();
        if (videoRect.contains(event->pos()))
        {
            m_startPoint = event->pos();
            m_endPoint = event->pos();
            m_drawing = true;
            m_hasSelection = false;
        }
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

        // Проверяем что выделение внутри области видео
        QRectF videoRect = getVideoAreaRect();
        if (videoRect.contains(m_startPoint) && videoRect.contains(m_endPoint))
        {
            int x = qMin(m_startPoint.x(), m_endPoint.x());
            int y = qMin(m_startPoint.y(), m_endPoint.y());
            int w = qAbs(m_endPoint.x() - m_startPoint.x());
            int h = qAbs(m_endPoint.y() - m_startPoint.y());

            if (w > 5 && h > 5)
            {
                m_selectedRect = QRect(x, y, w, h);
                m_hasSelection = true;
                emit rectangleSelected(m_selectedRect);
                // СРАЗУ очищаем выделение чтобы жёлтая рамка исчезла
                m_hasSelection = false;
                m_selectedRect = QRect();
            }
        }
        update();
    }
}

void VideoLabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event);

    if (m_frameImage.isNull()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Вычисляем размер с сохранением пропорций
    QSize targetSize = m_frameImage.size();
    targetSize.scale(size(), Qt::KeepAspectRatio);

    if (m_frameDirty || m_scaledSize != targetSize)
    {
        m_scaledImage = m_frameImage.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_scaledSize = targetSize;
        m_currentVideoSize = targetSize;
        m_frameDirty = false;
    }

    // Центрируем и СОХРАНЯЕМ смещение
    m_videoOffset.setX((width() - m_scaledSize.width()) / 2);
    m_videoOffset.setY((height() - m_scaledSize.height()) / 2);

    // Рисуем изображение
    painter.drawImage(m_videoOffset.x(), m_videoOffset.y(), m_scaledImage);

    // Рисуем прямоугольник ТОЛЬКО во время рисования (не после!)
    if (m_drawing)
    {
        painter.setPen(QPen(Qt::yellow, 2, Qt::SolidLine));
        painter.setBrush(Qt::NoBrush);

        int rx = qMin(m_startPoint.x(), m_endPoint.x());
        int ry = qMin(m_startPoint.y(), m_endPoint.y());
        int rw = qAbs(m_endPoint.x() - m_startPoint.x());
        int rh = qAbs(m_endPoint.y() - m_startPoint.y());

        painter.drawRect(QRect(rx, ry, rw, rh));
    }
}
