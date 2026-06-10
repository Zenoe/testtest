#pragma once

#include <QJsonObject>
#include <QString>

struct ControlResponse {
    bool ok = false;
    int code = 99;
    unsigned long win32 = 0;
    QString detail;
    QString serviceName;
    QJsonObject payload;
};

struct TrafficStatsResponse {
    bool ok = false;
    quint64 rxBytes = 0;
    quint64 txBytes = 0;
    qint64 lastHandshakeMsec = 0;
    QString detail;
};

class ControlServiceClient {
public:
    static bool isControllerInstalled();
    static ControlResponse ensureAvailable(const QString& serviceExePath);
    static ControlResponse send(const QJsonObject& request, int timeoutMs = 10'000);
    static TrafficStatsResponse queryTraffic(const QString& adapterName, int timeoutMs = 2'000);

private:
    static ControlResponse installElevated(const QString& serviceExePath);
};
