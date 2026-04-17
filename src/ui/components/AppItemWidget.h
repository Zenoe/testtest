#pragma once
#include <QWidget>
#include <QLabel>
#include "models/AppEntry.h"

class AppItemWidget : public QWidget {
    Q_OBJECT

public:
    explicit AppItemWidget(const AppEntry& entry, QWidget* parent = nullptr);

    const AppEntry& entry() const { return m_entry; }

signals:
    void activated(const AppEntry& entry);

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event)             override;
    void leaveEvent(QEvent* event)                  override;
    void mousePressEvent(QMouseEvent* event)        override;
    void mouseReleaseEvent(QMouseEvent* event)      override;
    void paintEvent(QPaintEvent* event)             override;

private:
    void setupUi();
    void setHovered(bool on);

    AppEntry m_entry;
    QLabel*  m_iconLabel;
    QLabel*  m_nameLabel;
    bool     m_pressed = false;
};
