#pragma once
#include <QWidget>
#include <QPixmap>
#include <QMenu>
#include "vpn/statusPanel.h"

class AvatarWidget : public QWidget {
    Q_OBJECT

public:
    explicit AvatarWidget(QWidget* parent = nullptr);

    void setAvatar(const QPixmap& pix);
    void setOnline(bool online);

signals:
    void clicked();
	void logoutRequested();

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    void showVpnStatus();

    QPixmap m_avatar;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_online = false;

    QMenu* m_menu = nullptr;
    StatusPanel* m_statusPanel = nullptr;
};