#pragma once
#include <QString>
#include <QPixmap>

struct AppEntry {
    QString  id;
    QString  displayName;
    QString  executablePath;
    QPixmap  icon;              // may be null until loaded async; use default then

    bool isValid() const { return !id.isEmpty() && !executablePath.isEmpty(); }
};
