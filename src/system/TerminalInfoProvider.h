#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QString>

struct TerminalInfoSnapshot {
    QString hardwareCode;
    QJsonObject payload;
};

class TerminalInfoProvider {
public:
    static TerminalInfoSnapshot collect();
    static QString hardwareCode();

private:
    static QJsonObject collectPcInfo();
    static QJsonObject collectOsInfo();
    static QJsonArray collectNetworkInfo();

    static QString terminalIp();
    static QString computerGroup();
    static QString remoteDesktopStatus();
    static QString terminalName();
    static QString serialNumber();
    static QString manufacturer();
    static QString model();
    static QString cpuModel();
    static QString memory();
    static QString hardDisk();
    static QString motherboard();
    static QString primaryMac();
    static QString virtualMachineStatus();
    static QString hotspotStatus();
    static QString lockPassword();

    static QString osModel();
    static QString osVersion();
    static QString osSerialNumber();
    static QString osInstallTime();
    static QString multiOsStatus();

    static QString defaultGatewayForInterface(const QString& interfaceName);
    static QString dnsForInterface(const QString& interfaceName);
    static QString networkNameForInterface(const QString& interfaceName);
};
