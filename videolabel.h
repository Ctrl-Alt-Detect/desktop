#ifndef VIDEOLABEL_H
#define VIDEOLABEL_H

#include <QLabel>
#include <QMouseEvent>
#include <QRect>
#include <QRectF>
#include <opencv2/opencv.hpp>

class VideoLabel : public QLabel
{
    Q_OBJECT
public:
    explicit VideoLabel(QWidget *parent = nullptr);
    void setFrame(const cv::Mat &frame);
    void clearSelection();
    QRectF getVideoAreaRect() const;  // Новый метод

signals:
    void rectangleSelected(QRect rect);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    cv::Mat m_rgbMat;
    QImage m_frameImage;
    QImage m_scaledImage;
    QSize m_scaledSize;
    QPoint m_startPoint;
    QPoint m_endPoint;
    QRect m_selectedRect;
    bool m_drawing;
    bool m_hasSelection;
    bool m_frameDirty;
    QPoint m_videoOffset;      // Смещение видео
    QSize m_currentVideoSize;  // Размер видео
};

#endif // VIDEOLABEL_H
