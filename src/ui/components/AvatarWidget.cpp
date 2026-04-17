#include "AvatarWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QMenu>
#include <QPainterPath>
#include "SettingsDialog.h"

AvatarWidget::AvatarWidget(QWidget* parent) : QWidget(parent)
{
    setFixedSize(40, 40);  
    setCursor(Qt::PointingHandCursor);

    m_menu = new QMenu(this);
    QAction* settingsAction = m_menu->addAction("Settings");
    connect(settingsAction, &QAction::triggered, this, [this]() {
        auto* dlg = new SettingsDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
        });
    m_menu->addSeparator();
    m_menu->addAction("Quit", qApp, &QApplication::quit);

    //m_menu->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    //m_menu->setAttribute(Qt::WA_TranslucentBackground);
    //m_menu->setAttribute(Qt::WA_NoSystemBackground, true); 
}

void AvatarWidget::setAvatar(const QPixmap& pix) {
	m_avatar = pix;
	update();
}

void AvatarWidget::setOnline(bool online) {
    m_online = online;
    update();
}

void AvatarWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect r = rect();

    // 背景（hover / pressed）
    if (m_pressed)
        p.fillRect(r, QColor(220, 220, 220));
    else if (m_hovered)
        p.fillRect(r, QColor(240, 240, 240));

    //️ 头像（圆形裁剪）
    QPainterPath path;
    path.addEllipse(r.adjusted(2, 2, -2, -2));
    p.setClipPath(path);

    if (!m_avatar.isNull()) {
        p.drawPixmap(r, m_avatar);
    }

    p.setClipping(false);

    // 在线状态点
    if (m_online) {
        QRect dot(26, 26, 10, 10);
        p.setBrush(Qt::green);
        p.setPen(Qt::white);
        p.drawEllipse(dot);
    }
}

void AvatarWidget::enterEvent(QEnterEvent*) {
    m_hovered = true;
    update();
}

void AvatarWidget::leaveEvent(QEvent*) {
    m_hovered = false;
    m_pressed = false;
    update();
}

void AvatarWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
        // 弹菜单（像 Explorer）
        m_menu->exec(mapToGlobal(QPoint(0, height())));
    }
}