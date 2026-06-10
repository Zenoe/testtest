#pragma once

#include <QJsonObject>
#include <QString>

struct ControlResponse {
    bool ok = false;
    int code = 99;
    unsigned long win32 = 0;
    QString detail;
    QString serviceName;
};

class ControlServiceClient {
public:
    static bool isControllerInstalled();
    static ControlResponse ensureAvailable(const QString& serviceExePath);
    static ControlResponse send(const QJsonObject& request, int timeoutMs = 10'000);

private:
    static ControlResponse installElevated(const QString& serviceExePath);
};
