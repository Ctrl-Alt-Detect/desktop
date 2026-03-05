#include "WindowControls.h"
#include <QHBoxLayout>

#include "outlinebutton.h"
#include "outlinebuttontoggle.h"

WindowControls::WindowControls(QWidget *parent) : QWidget(parent) {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_btnMin   = makeButton("−", "#2a7a2a");
    m_btnMax   = makeToggleButton("□", "#2a5a9a");
    m_btnClose = makeButton("×", "#9a2a2a");

    connect(m_btnMin,   &QPushButton::clicked, this, &WindowControls::minimizeRequested);
    connect(m_btnMax,   &QPushButton::clicked, this, &WindowControls::maximizeRequested);
    connect(m_btnClose, &QPushButton::clicked, this, &WindowControls::closeRequested);

    layout->addWidget(m_btnMin);
    layout->addWidget(m_btnMax);
    layout->addWidget(m_btnClose);
}

OutlineButton* WindowControls::makeButton(const QString &symbol, const QString &hoverColor) {
    auto *btn = new OutlineButton(symbol);
    btn->setFixedSize(30, 24);
    btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(QString(
        "QPushButton {"
        "  background: rgba(255,255,255,0.08);"
        "  border-radius: 8px;"
        "  border: 1px solid rgba(255,255,255,0.12);"
        "  color: #aabbcc;"
        "  font-size: 12px;"
        "  padding: 0px;"
        "}"
        "QPushButton:hover {"
        "  background: %1;"
        "  color: #ffffff;"
        "}"
    ).arg(hoverColor));
    return btn;
}

OutlineButtonToggle* WindowControls::makeToggleButton(const QString &symbol, const QString &hoverColor) {
    auto *btn = new OutlineButtonToggle(symbol);
    btn->setFixedSize(30, 24);
    btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(QString(
        "QPushButton {"
        "  background: rgba(255,255,255,0.08);"
        "  border-radius: 8px;"
        "  border: 1px solid rgba(255,255,255,0.12);"
        "  color: #aabbcc;"
        "  font-size: 12px;"
        "  padding: 0px;"
        "}"
        "QPushButton:hover {"
        "  background: %1;"
        "  color: #ffffff;"
        "}"
    ).arg(hoverColor));
    return btn;
}
