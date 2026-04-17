#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QMap>
#include "ui/navigation/NavItem.h"

class NavigationWidget : public QWidget {
    Q_OBJECT

public:
    explicit NavigationWidget(QWidget* parent = nullptr);

    void setActiveItem(const QString& id);
    void setUsername(const QString& name);   // updates avatar tooltip/initials

signals:
    void itemSelected(const QString& id);

private:
    void setupUi();
    void addNavItem(const QString& id, const QString& iconPath,
                    const QString& tooltip);

    QVBoxLayout*          m_layout;
    //QLabel*               m_avatarLabel;
    QMap<QString, NavItem*> m_items;
    QString               m_activeId;


    QPixmap makeAvatarPixmap(const QChar& initial,
                             const QColor& bgColor) const;
};
