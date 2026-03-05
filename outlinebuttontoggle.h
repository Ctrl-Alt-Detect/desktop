#pragma once
#include "OutlineButton.h"

class OutlineButtonToggle : public OutlineButton {
    Q_OBJECT

    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)

public:
    explicit OutlineButtonToggle(const QString &text, QWidget *parent = nullptr);

    bool isActive() const { return m_active; }
    void setActive(bool active);

signals:
    void activeChanged(bool active);

private:
    bool m_active = false;
};
