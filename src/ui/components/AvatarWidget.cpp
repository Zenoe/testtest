#include "AvatarWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QMenu>
#include <QPainterPath>
#include "SettingsDialog.h"
#include "backend/SessionManager.h"
#include <QFileInfo>
#include "utils/logger.h"

// ---------------------------------------------------------------------------
// Locate the true MainWindow reliably, without relying on topLevelWidgets()
// ordering.  AvatarWidget lives somewhere deep in the hierarchy — just walk
// up, same logic as StatusPanel::resolveAnchorWindow().
// ---------------------------------------------------------------------------
static QWidget* findMainWindow(QWidget* start)
{
    QWidget* w = start;
    while (w && w->parentWidget())
        w = w->parentWidget();

    if (!w)
        spdlog::error("findMainWindow: could not resolve a top-level window from {}",
            start ? start->metaObject()->className() : "<null>");
    else
        spdlog::debug("findMainWindow → '{}' ({})",
            w->windowTitle().toStdString(),
            w->metaObject()->className());
    return w;
}

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
     m_menu->addAction("Vpn Status", this, &AvatarWidget::showVpnStatus);
    //QAction* logoutAction = m_menu->addAction("Logout", this, &AvatarWidget::logoutRequested);
	QAction* logoutAction = m_menu->addAction("Logout", this, [this]() { emit logoutRequested(); });
    m_menu->addSeparator();
    m_menu->addAction("Quit", this, [this]() { emit logoutRequested(); qApp->quit(); });

    //m_menu->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    //m_menu->setAttribute(Qt::WA_TranslucentBackground);
    //m_menu->setAttribute(Qt::WA_NoSystemBackground, true); 
}

void AvatarWidget::showVpnStatus() {
    if (!m_statusPanel) {
        const QString confPath = SessionManager::instance().vpnConfPath();
        if (confPath.isEmpty()) {
            spdlog::warn("AvatarWidget::showVpnStatus: vpnConf() is empty — aborting");
            return;
        }

        QWidget* mainWin = findMainWindow(this);   // ← correct anchor
        if (!mainWin) {
            spdlog::error("AvatarWidget::showVpnStatus: no top-level window found");
            return;
        }

        m_statusPanel = new StatusPanel(QFileInfo(confPath).baseName(), mainWin);

        // If MainWindow is ever destroyed, null our pointer so we don't dangle.
        connect(mainWin, &QObject::destroyed,
            this, [this]() {
                //spdlog::warn("AvatarWidget: MainWindow destroyed — clearing panel ref"); can not log here, spdlog has been destroy
                m_statusPanel = nullptr;
            });

        spdlog::info("AvatarWidget: StatusPanel created (conf='{}')",
            QFileInfo(confPath).baseName().toStdString());
    }

    // ── Toggle visibility ───────────────────────────────────────────────────
    m_statusPanel->toggle();
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
