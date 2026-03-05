#pragma once
#include <QPushButton>

class OutlineButton : public QPushButton {
    Q_OBJECT
public:
    explicit OutlineButton(const QString &text, QWidget *parent = nullptr);

protected:
    void applyNormalStyle();
    void applyActiveStyle();
};
