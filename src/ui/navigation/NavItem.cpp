#include "NavItem.h"
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QEnterEvent>

NavItem::NavItem(const QString& id,
                 const QIcon&   icon,
                 const QString& tooltip,
                 QWidget*       parent)
    : QWidget(parent)
    , m_id(id)
{
    setFixedSize(48, 48);
    setToolTip(tooltip);
    setCursor(Qt::PointingHandCursor);

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setAlignment(Qt::AlignCenter);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(24, 24);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setPixmap(
        icon.pixmap(24, 24));           // crisp at logical 24×24
    m_iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    vl->addWidget(m_iconLabel);
}

// ── State ─────────────────────────────────────────────────────────────────────

void NavItem::setActive(bool on) {
    if (m_active == on) return;
    m_active = on;
    update();                           // triggers paintEvent
}

// ── Events ────────────────────────────────────────────────────────────────────

void NavItem::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton)
        emit clicked(m_id);
    QWidget::mousePressEvent(event);
}

void NavItem::enterEvent(QEnterEvent* event) {
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void NavItem::leaveEvent(QEvent* event) {
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void NavItem::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect r = rect().adjusted(4, 4, -4, -4);   // inset so highlight has padding

    if (m_active) {
        // Active: filled blue pill
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x24, 0x68, 0xF2, 230));
        QPainterPath path;
        path.addRoundedRect(r, 8, 8);
        p.fillPath(path, p.brush());

        // Tint the icon white by drawing a white overlay via composition
        QPixmap tinted = tintedIcon(QColor(255, 255, 255));
        const QPoint centre = rect().center() - QPoint(12, 12);
        p.drawPixmap(centre, tinted);

    } else if (m_hovered) {
        // Hover: subtle grey background
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 18));
        QPainterPath path;
        path.addRoundedRect(r, 8, 8);
        p.fillPath(path, p.brush());

        // Let icon render normally (already set on m_iconLabel)
        QWidget::paintEvent(event);

    } else {
        // Default: just the child label renders
        QWidget::paintEvent(event);
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

QPixmap NavItem::tintedIcon(const QColor& color) const {
    QPixmap src = m_iconLabel->pixmap();
    if (src.isNull()) return {};

    QPixmap result(src.size());
    result.fill(Qt::transparent);

    QPainter p(&result);
    p.drawPixmap(0, 0, src);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(result.rect(), color);
    p.end();

    return result;
}
