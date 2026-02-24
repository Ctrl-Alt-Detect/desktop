#ifndef VIDEOLABEL_H
#define VIDEOLABEL_H

#include <QLabel>
#include <QMouseEvent>
#include <QRect>
#include <QImage>
#include <QSize>
#include <opencv2/opencv.hpp>

class VideoLabel : public QLabel
{
    Q_OBJECT

public:
    explicit VideoLabel(QWidget *parent = nullptr);
    
    void setFrame(const cv::Mat &frame);
    QRect getSelectedRect() const { return m_selectedRect; }
    bool hasSelection() const { return m_hasSelection; }
    void clearSelection();

signals:
    void rectangleSelected(QRect rect);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QPoint m_startPoint;
    QPoint m_endPoint;
    bool m_drawing;
    bool m_hasSelection;
    QRect m_selectedRect;
    cv::Mat m_rgbMat;
    QImage m_frameImage;
    QImage m_scaledImage;
    QSize m_scaledSize;
    bool m_frameDirty = false;
};

#endif // VIDEOLABEL_H
