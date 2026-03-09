#include "OutlineButton.h"

OutlineButton::OutlineButton(const QString &text, QWidget *parent)
    : QPushButton(text, parent)
{
    setFixedHeight(42);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    applyNormalStyle();
}

void OutlineButton::applyNormalStyle() {
    setStyleSheet(
        "QPushButton {"
        "  background: #1e2d3d;"
        "  color: #aabbcc;"
        "  border: 1px solid #2a3d52;"
        "  border-radius: 18px;"
        "  padding: 8px 20px;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "  background: #253545;"
        "  color: #ffffff;"
        "}"
        "QPushButton:pressed {"
        "  background: #1a2a3a;"
        "  color: #00aaff;"
        "  border: 1px solid #00aaff;"
        "}"
    );
}

void OutlineButton::applyActiveStyle() {
    setStyleSheet(
        "QPushButton {"
        "  background: #1a2a3a;"
        "  color: #00aaff;"
        "  border: 2px solid #00aaff;"
        "  border-radius: 18px;"
        "  padding: 8px 20px;"
        "  font-weight: bold;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "  background: #1e3a50;"
        "  color: #33bbff;"
        "  border: 2px solid #33bbff;"
        "}"
    );
}
