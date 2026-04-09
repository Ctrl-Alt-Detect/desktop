#pragma once
#include <QLabel>
#include <QRect>

class VideoLabel : public QLabel {
    Q_OBJECT

public:
    explicit VideoLabel(QWidget* parent = nullptr);
    void setFrameSize(int width, int height);

signals:
    void selectionMade(int x, int y, int width, int height);
    void trackerReset();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QPoint mapToFrameCoords(const QPoint& widgetPos) const;

    QPoint m_startPoint;
    QPoint m_currentPoint;
    bool m_selecting{false};
    int m_frameWidth{0};
    int m_frameHeight{0};
};
