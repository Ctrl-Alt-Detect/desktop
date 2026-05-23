#pragma once

#include <QWidget>
#include <QSlider>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QEvent>
#include <QPoint>
#include <QLabel>

class SeekSlider : public QSlider {
    Q_OBJECT

public:
    explicit SeekSlider(QWidget* parent = nullptr);

    void setMetadata(int totalFrames, double fps);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    int valueFromPosition(const QPoint& position) const;
    void updateHoverTooltip(const QPoint& globalPos, int frameIndex);
    QString frameToTimeText(int frameIndex) const;

    int m_totalFrames{0};
    double m_fps{0.0};
};

class SeekBar : public QWidget {
    Q_OBJECT

public:
    explicit SeekBar(QWidget* parent = nullptr);
    ~SeekBar() override;

    // Slider control methods
    void setRange(int min, int max);
    void setValue(int value);
    int value() const;
    int maximum() const;
    bool isSeeking() const;

    // Play button methods
    void setPlayButtonText(const QString& text);
    QString playButtonText() const;

    // Metadata for tooltips
    void setMetadata(int totalFrames, double fps);

signals:
    void sliderReleased();
    void valueChanged(int value);
    void playPauseClicked();

protected:
private slots:
    void onPlayPauseClicked();
    void onSliderValueChanged(int value);
    void onSliderReleased();

private:
    void updateTimeLabels();

    QHBoxLayout* m_layout;
    QPushButton* m_playButton;
    SeekSlider* m_slider;
    QLabel* m_currentTimeLabel;
    QLabel* m_totalTimeLabel;
    int m_totalFrames{0};
    double m_fps{0.0};
};
