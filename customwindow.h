#ifndef _CUSTOM_WINDOW_H
#define _CUSTOM_WINDOW_H

#include <QWidget>
#include <QMouseEvent>

class QVBoxLayout;
class QLabel;
class MainWindow;

class CustomWindow : public QWidget {
    Q_OBJECT
public:
    explicit CustomWindow(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void changeEvent(QEvent *event) override;

private:
    void updateWindowChrome();

    QVBoxLayout *m_outerLayout = nullptr;
    QWidget *m_container = nullptr;
    QLabel *m_titleLabel = nullptr;
    MainWindow *m_mainWindow = nullptr;
};

#endif//_CUSTOM_WINDOW_H
