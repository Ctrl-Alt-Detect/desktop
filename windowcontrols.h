#pragma once
#include <QWidget>

class OutlineButton;
class OutlineButtonToggle;

class WindowControls : public QWidget {
    Q_OBJECT
public:
    explicit WindowControls(QWidget *parent = nullptr);

signals:
    void minimizeRequested();
    void maximizeRequested();
    void closeRequested();

private:
    OutlineButton *m_btnMin;
    OutlineButtonToggle *m_btnMax;
    OutlineButton *m_btnClose;

    static OutlineButton* makeButton(const QString &symbol, const QString &hoverColor);
    static OutlineButtonToggle* makeToggleButton(const QString &symbol, const QString &hoverColor);
};
