#include "AppItemWidget.h"
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle> // 添加此头文件以解决 QStyle 不完整类型问题

AppItemWidget::AppItemWidget(const AppEntry& entry, QWidget* parent)
    : QWidget(parent)
    , m_entry(entry)
{
    setupUi();
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void AppItemWidget::setupUi() {
    setObjectName("AppTile");
    setFixedSize(100, 100);
    setCursor(Qt::PointingHandCursor);
    setToolTip(m_entry.displayName);

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(8, 12, 8, 8);
    vl->setSpacing(6);
    vl->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    // Icon
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(48, 48);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setScaledContents(true);
    m_iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_iconLabel->setPixmap(
        m_entry.icon.isNull()
            ? QPixmap(":/icons/default_app.png")
            : m_entry.icon);

    // Name label — elide if too long
    m_nameLabel = new QLabel(this);
    m_nameLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_nameLabel->setWordWrap(false);
    m_nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_nameLabel->setObjectName("AppTileLabel");

    // Elide display name to fit tile width
    QFontMetrics fm(m_nameLabel->font());
    const QString elided = fm.elidedText(
        m_entry.displayName, Qt::ElideRight, width() - 16);
    m_nameLabel->setText(elided);

    vl->addWidget(m_iconLabel, 0, Qt::AlignHCenter);
    vl->addWidget(m_nameLabel, 0, Qt::AlignHCenter);
}

// ── Public API ────────────────────────────────────────────────────────────────

void AppItemWidget::setHovered(bool on) {
    setProperty("hovered", on);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

// ── Events ────────────────────────────────────────────────────────────────────

void AppItemWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton)
        emit activated(m_entry);
    QWidget::mouseDoubleClickEvent(event);
}

void AppItemWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
    }
    QWidget::mousePressEvent(event);
}

void AppItemWidget::mouseReleaseEvent(QMouseEvent* event) {
    m_pressed = false;
    update();
    QWidget::mouseReleaseEvent(event);
}

void AppItemWidget::enterEvent(QEnterEvent* event) {
    setHovered(true);
    QWidget::enterEvent(event);
}

void AppItemWidget::leaveEvent(QEvent* event) {
    m_pressed = false;
    setHovered(false);
    QWidget::leaveEvent(event);
}

void AppItemWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect r = rect().adjusted(2, 2, -2, -2);
    QPainterPath path;
    path.addRoundedRect(r, 8, 8);

    if (m_pressed) {
        p.fillPath(path, QColor(0x24, 0x68, 0xF2, 40));
    } else if (property("hovered").toBool()) {
        p.fillPath(path, QColor(0x24, 0x68, 0xF2, 20));
    }
    // else: transparent — let parent background show through

    // Draw children (icon + label) on top
    // We must call the base class paintEvent to render child widgets
    QWidget::paintEvent(event);
}
