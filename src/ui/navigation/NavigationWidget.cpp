#include "NavigationWidget.h"
#include <QLabel>
#include <QSpacerItem>
#include <QPainter>
#include <QPainterPath>
#include <QMenu>
#include <QEvent>
#include <QMouseEvent>
#include <QApplication>
#include "ui/components/SettingsDialog.h"
#include "ui/components/AvatarWidget.h"
NavigationWidget::NavigationWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void NavigationWidget::setupUi() {
    setObjectName("NavigationWidget");
    setFixedWidth(64);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 16, 8, 16);
    m_layout->setSpacing(4);
    m_layout->setAlignment(Qt::AlignTop);

    // ── Avatar (top) ──────────────────────────────────────────────────────────
    //m_avatarLabel = new QLabel(this);
    //m_avatarLabel->setFixedSize(40, 40);
    //m_avatarLabel->setAlignment(Qt::AlignCenter);
    //m_avatarLabel->setObjectName("AvatarLabel");
    //m_avatarLabel->setToolTip("Profile");
    //QPixmap pix(":/avatar.svg");
    //m_avatarLabel->setPixmap(pix.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    //m_avatarLabel->setPixmap(makeAvatarPixmap(QChar('?'), QColor(0x24, 0x68, 0xF2)));

    AvatarWidget* avatar = new AvatarWidget;
    avatar->setAvatar(QPixmap(":/avatar.svg"));
    //avatar->setOnline(true);
    m_layout->addWidget(avatar, 0, Qt::AlignHCenter);
    m_layout->addSpacing(16);

    // ── Navigation items ──────────────────────────────────────────────────────
    addNavItem("security",  ":/security.svg",  "Security");
    addNavItem("tools",     ":/tools.svg",     "Tools");

    // Push app-store to the bottom
    m_layout->addStretch();
    addNavItem("store", ":/store.svg", "App store");
}

// ── Public API ────────────────────────────────────────────────────────────────

void NavigationWidget::setActiveItem(const QString& id) {
    if (m_activeId == id) return;

    // Deactivate previous
    if (m_items.contains(m_activeId))
        m_items[m_activeId]->setActive(false);

    // Activate new
    m_activeId = id;
    if (m_items.contains(id))
        m_items[id]->setActive(true);
}

void NavigationWidget::setUsername(const QString& name) {
    // Use first letter of name as avatar initial; fall back to '?'
    const QChar initial = name.isEmpty() ? QChar('?') : name.at(0).toUpper();
    //m_avatarLabel->setPixmap(
    //    makeAvatarPixmap(initial, QColor(0x24, 0x68, 0xF2)));
    //m_avatarLabel->setToolTip(name);
}

// ── Private helpers ───────────────────────────────────────────────────────────

void NavigationWidget::addNavItem(const QString& id,
                                  const QString& iconPath,
                                  const QString& tooltip)
{
    const QIcon icon(iconPath);
    auto* item = new NavItem(id, icon, tooltip, this);

    // Relay NavItem::clicked → NavigationWidget::itemSelected
    connect(item, &NavItem::clicked, this, [this](const QString& itemId) {
        setActiveItem(itemId);
        emit itemSelected(itemId);
    });

    m_items.insert(id, item);
    m_layout->addWidget(item, 0, Qt::AlignHCenter);
}

QPixmap NavigationWidget::makeAvatarPixmap(const QChar& initial,
                                           const QColor& bgColor) const
{
    constexpr int size = 40;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    // Circular background
    p.setPen(Qt::NoPen);
    p.setBrush(bgColor);
    p.drawEllipse(0, 0, size, size);

    // Initial letter
    QFont f = p.font();
    f.setPixelSize(16);
    f.setWeight(QFont::Medium);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 0, size, size),
               Qt::AlignCenter,
               QString(initial));

    p.end();
    return pix;
}
