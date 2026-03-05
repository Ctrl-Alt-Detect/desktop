#include "CustomWindow.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QWindow>
#include <QCursor>
#include <QEvent>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "mainwindow.h"
#include "windowcontrols.h"

CustomWindow::CustomWindow(QWidget *parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NativeWindow);
    setMinimumSize(900, 560);

    m_outerLayout = new QVBoxLayout(this);
    m_outerLayout->setContentsMargins(20, 20, 20, 20);

    m_container = new QWidget;
    m_container->setObjectName("container");
    m_container->setStyleSheet(
        "#container {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "    stop:0 #0d1b2a, stop:1 #1a2a3a);"
        "  border-radius: 16px;"
        "  border: 2px solid rgba(255,255,255,0.12);"
        "}"
    );

    auto *containerLayout = new QVBoxLayout(m_container);
    containerLayout->setContentsMargins(12, 10, 12, 12);
    containerLayout->setSpacing(0);

    auto *titleBar = new QHBoxLayout;
    titleBar->setContentsMargins(8, 0, 0, 0);
    titleBar->setSpacing(8);

    m_titleLabel = new QLabel("Computer Vision Tracker", m_container);
    m_titleLabel->setStyleSheet(
        "QLabel {"
        "  color: #e6eef7;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "  padding: 4px 8px;"
        "}"
    );
    titleBar->addWidget(m_titleLabel);
    titleBar->addStretch();

    auto *controls = new WindowControls;
    connect(controls, &WindowControls::minimizeRequested, this, &QWidget::showMinimized);
    connect(controls, &WindowControls::maximizeRequested, this, [this]{
        isMaximized() ? showNormal() : showMaximized();
    });
    connect(controls, &WindowControls::closeRequested, this, &QWidget::close);

    titleBar->addWidget(controls);
    containerLayout->addLayout(titleBar);

    m_mainWindow = new MainWindow(m_container);
    m_mainWindow->setWindowFlags(Qt::Widget);
    m_mainWindow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    containerLayout->addWidget(m_mainWindow, 1);

    m_outerLayout->addWidget(m_container);
    updateWindowChrome();
}

void CustomWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (QWindow *win = windowHandle())
        {
            win->startSystemMove();
        }
    }
    QWidget::mousePressEvent(event);
}

bool CustomWindow::nativeEvent(const QByteArray &eventType, void *message, long *result) {
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_NCHITTEST) {
            const int resizeBorder = 8;
            const int margin = isMaximized() ? 0 : 12;
            const QPoint localPos = mapFromGlobal(QCursor::pos());

            // Only detect resize if cursor is within the actual container bounds (excluding shadow margin)
            if (localPos.x() < margin || localPos.x() >= width() - margin ||
                localPos.y() < margin || localPos.y() >= height() - margin) {
                return QWidget::nativeEvent(eventType, message, result);
            }

            const bool onLeft = localPos.x() >= margin && localPos.x() < margin + resizeBorder;
            const bool onRight = localPos.x() <= width() - margin && localPos.x() > width() - margin - resizeBorder;
            const bool onTop = localPos.y() >= margin && localPos.y() < margin + resizeBorder;
            const bool onBottom = localPos.y() <= height() - margin && localPos.y() > height() - margin - resizeBorder;

            if (onTop && onLeft) {
                *result = HTTOPLEFT;
                return true;
            }
            if (onTop && onRight) {
                *result = HTTOPRIGHT;
                return true;
            }
            if (onBottom && onLeft) {
                *result = HTBOTTOMLEFT;
                return true;
            }
            if (onBottom && onRight) {
                *result = HTBOTTOMRIGHT;
                return true;
            }
            if (onLeft) {
                *result = HTLEFT;
                return true;
            }
            if (onRight) {
                *result = HTRIGHT;
                return true;
            }
            if (onTop) {
                *result = HTTOP;
                return true;
            }
            if (onBottom) {
                *result = HTBOTTOM;
                return true;
            }
        }
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return QWidget::nativeEvent(eventType, message, result);
}

void CustomWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::WindowStateChange) {
        updateWindowChrome();
    }
    QWidget::changeEvent(event);
}

void CustomWindow::updateWindowChrome() {
    const bool edgeToEdge = isMaximized() || isFullScreen();

    if (m_outerLayout != nullptr) {
        m_outerLayout->setContentsMargins(edgeToEdge ? 0 : 12, edgeToEdge ? 0 : 12, edgeToEdge ? 0 : 12, edgeToEdge ? 0 : 12);
    }
    if (m_container != nullptr) {
        m_container->setStyleSheet(
            edgeToEdge
                ? "#container {"
                  "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                  "    stop:0 #0d1b2a, stop:1 #1a2a3a);"
                  "  border-radius: 0px;"
                  "  border: none;"
                  "}"
                : "#container {"
                  "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                  "    stop:0 #0d1b2a, stop:1 #1a2a3a);"
                  "  border-radius: 16px;"
                  "  border: 2px solid rgba(255,255,255,0.12);"
                  "}"
        );
    }

    setAttribute(Qt::WA_TranslucentBackground, !edgeToEdge);
    update();
}
