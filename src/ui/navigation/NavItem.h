#pragma once
#include <QWidget>
#include <QLabel>
#include <QIcon>

class NavItem : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive WRITE setActive)

public:
    explicit NavItem(const QString& id,
                     const QIcon&   icon,
                     const QString& tooltip,
                     QWidget*       parent = nullptr);

    QString id()      const { return m_id; }
    bool    isActive() const { return m_active; }
    void    setActive(bool on);

signals:
    void clicked(const QString& id);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event)      override;
    void leaveEvent(QEvent* event)           override;
    void paintEvent(QPaintEvent* event)      override;

    QPixmap tintedIcon(const QColor& color) const;

private:
    QString  m_id;
    QLabel*  m_iconLabel;
    bool     m_active  = false;
    bool     m_hovered = false;
};
