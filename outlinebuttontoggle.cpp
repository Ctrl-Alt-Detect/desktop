#include "OutlineButtonToggle.h"

OutlineButtonToggle::OutlineButtonToggle(const QString &text, QWidget *parent)
    : OutlineButton(text, parent)
{
    connect(this, &QPushButton::clicked, this, [this]{
        setActive(!m_active);
    });
}

void OutlineButtonToggle::setActive(bool active) {
    if (m_active == active) return;
    m_active = active;
    active ? applyActiveStyle() : applyNormalStyle();
    emit activeChanged(active);
}
