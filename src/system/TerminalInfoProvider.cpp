#include "TerminalInfoProvider.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QNetworkInterface>
#include <QSettings>
#include <QStorageInfo>
#include <QSysInfo>
#include <QtMath>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

#include "utils/logger.h"

namespace {

QString registryValue(const QString& path, const QString& key)
{
#ifdef Q_OS_WIN
    QSettings settings(path, QSettings::NativeFormat);
    return settings.value(key).toString().trimmed();
#else
    Q_UNUSED(path);
    Q_UNUSED(key);
    return {};
#endif
}

QString formatBytes(quint64 bytes)
{
    static constexpr quint64 kGiB = 1024ULL * 1024ULL * 1024ULL;
    if (bytes == 0)
        return {};
    return QStringLiteral("%1GB").arg(qRound64(static_cast<double>(bytes) / kGiB));
}

bool isUsableInterface(const QNetworkInterface& networkInterface)
{
    return networkInterface.isValid()
        && !(networkInterface.flags() & QNetworkInterface::IsLoopBack);
}

bool isActiveInterface(const QNetworkInterface& networkInterface)
{
    return isUsableInterface(networkInterface)
        && (networkInterface.flags() & QNetworkInterface::IsUp)
        && (networkInterface.flags() & QNetworkInterface::IsRunning);
}

QString firstAddress(const QNetworkInterface& networkInterface,
                     QAbstractSocket::NetworkLayerProtocol protocol)
{
    for (const auto& entry : networkInterface.addressEntries()) {
        const QHostAddress address = entry.ip();
        if (address.protocol() == protocol && !address.isLoopback())
            return address.toString();
    }
    return {};
}

} // namespace

TerminalInfoSnapshot TerminalInfoProvider::collect()
{
    TerminalInfoSnapshot snapshot;
    snapshot.hardwareCode = hardwareCode();
    snapshot.payload = {
        { "terminalIp", terminalIp() },
        { "clientVersion", QCoreApplication::applicationVersion() },
        { "pcInfo", collectPcInfo() },
        { "osInfo", collectOsInfo() },
        { "networkInfo", collectNetworkInfo() },
    };

    spdlog::info("TerminalInfoProvider: collected terminal information, networkAdapters={}",
        snapshot.payload.value("networkInfo").toArray().size());
    return snapshot;
}

QString TerminalInfoProvider::hardwareCode()
{
    QByteArray source = QSysInfo::machineUniqueId();
    if (source.isEmpty()) {
        spdlog::warn("TerminalInfoProvider: machineUniqueId unavailable, using fallback identity");
        source = QStringLiteral("%1|%2|%3")
                     .arg(terminalName(), manufacturer(), model())
                     .toUtf8();
    }

    if (source.isEmpty()) {
        spdlog::error("TerminalInfoProvider: no stable hardware identity is available");
        return {};
    }

    return QString::fromLatin1(
        QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex().toUpper());
}

QJsonObject TerminalInfoProvider::collectPcInfo()
{
    return {
        { "computerGroup", computerGroup() },
        { "remoteDesktop", remoteDesktopStatus() },
        { "terminalName", terminalName() },
        { "hardwareCode", hardwareCode() },
        { "serialNumber", serialNumber() },
        { "manufacturer", manufacturer() },
        { "model", model() },
        { "cpuModel", cpuModel() },
        { "memory", memory() },
        { "hardDisk", hardDisk() },
        { "motherboard", motherboard() },
        { "mac", primaryMac() },
        { "isVm", virtualMachineStatus() },
        { "hotspotStatus", hotspotStatus() },
        { "lockPassword", lockPassword() },
    };
}

QJsonObject TerminalInfoProvider::collectOsInfo()
{
    return {
        { "osModel", osModel() },
        { "osVersion", osVersion() },
        { "osSn", osSerialNumber() },
        { "osInstallTime", osInstallTime() },
        { "isMultiOs", multiOsStatus() },
    };
}

QJsonArray TerminalInfoProvider::collectNetworkInfo()
{
    QJsonArray result;
    for (const auto& networkInterface : QNetworkInterface::allInterfaces()) {
        if (!isUsableInterface(networkInterface))
            continue;

        const QString interfaceName = networkInterface.humanReadableName();
        result.append(QJsonObject{
            { "networkCardName", interfaceName },
            { "mac", networkInterface.hardwareAddress() },
            { "ipv4", firstAddress(networkInterface, QAbstractSocket::IPv4Protocol) },
            { "ipv6", firstAddress(networkInterface, QAbstractSocket::IPv6Protocol) },
            { "defaultGateway", defaultGatewayForInterface(interfaceName) },
            { "dns", dnsForInterface(interfaceName) },
            { "networkName", networkNameForInterface(interfaceName) },
            { "status", isActiveInterface(networkInterface) ? QStringLiteral("开启")
                                                             : QStringLiteral("关闭") },
        });
    }
    return result;
}

QString TerminalInfoProvider::terminalIp()
{
    for (const auto& networkInterface : QNetworkInterface::allInterfaces()) {
        if (!isActiveInterface(networkInterface))
            continue;
        const QString ipv4 = firstAddress(networkInterface, QAbstractSocket::IPv4Protocol);
        if (!ipv4.isEmpty())
            return ipv4;
    }
    return {};
}

QString TerminalInfoProvider::computerGroup()
{
    return {};
}

QString TerminalInfoProvider::remoteDesktopStatus()
{
#ifdef Q_OS_WIN
    QSettings settings(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Terminal Server"),
        QSettings::NativeFormat);
    if (!settings.contains("fDenyTSConnections"))
        return QStringLiteral("未知");
    return settings.value("fDenyTSConnections").toInt() == 0
        ? QStringLiteral("是")
        : QStringLiteral("否");
#else
    return QStringLiteral("未知");
#endif
}

QString TerminalInfoProvider::terminalName()
{
    return QSysInfo::machineHostName();
}

QString TerminalInfoProvider::serialNumber()
{
    return registryValue(
        QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\BIOS"),
        QStringLiteral("SystemSerialNumber"));
}

QString TerminalInfoProvider::manufacturer()
{
    return registryValue(
        QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\BIOS"),
        QStringLiteral("SystemManufacturer"));
}

QString TerminalInfoProvider::model()
{
    return registryValue(
        QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\BIOS"),
        QStringLiteral("SystemProductName"));
}

QString TerminalInfoProvider::cpuModel()
{
    return registryValue(
        QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"),
        QStringLiteral("ProcessorNameString"));
}

QString TerminalInfoProvider::memory()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
        return formatBytes(status.ullTotalPhys);
#endif
    return {};
}

QString TerminalInfoProvider::hardDisk()
{
    const QStorageInfo root = QStorageInfo::root();
    return root.isValid() && root.isReady() ? formatBytes(root.bytesTotal()) : QString{};
}

QString TerminalInfoProvider::motherboard()
{
    const QString manufacturer = registryValue(
        QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\BIOS"),
        QStringLiteral("BaseBoardManufacturer"));
    const QString product = registryValue(
        QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\BIOS"),
        QStringLiteral("BaseBoardProduct"));
    return QStringLiteral("%1 %2").arg(manufacturer, product).trimmed();
}

QString TerminalInfoProvider::primaryMac()
{
    for (const auto& networkInterface : QNetworkInterface::allInterfaces()) {
        if (isActiveInterface(networkInterface) && !networkInterface.hardwareAddress().isEmpty())
            return networkInterface.hardwareAddress();
    }
    return {};
}

QString TerminalInfoProvider::virtualMachineStatus()
{
    const QString identity = QStringLiteral("%1 %2").arg(manufacturer(), model()).toLower();
    if (identity.isEmpty())
        return QStringLiteral("未知");

    static const QStringList vmMarkers{
        "virtual", "vmware", "virtualbox", "qemu", "kvm", "hyper-v", "xen", "parallels"
    };
    for (const auto& marker : vmMarkers) {
        if (identity.contains(marker))
            return QStringLiteral("是");
    }
    return QStringLiteral("否");
}

QString TerminalInfoProvider::hotspotStatus()
{
    for (const auto& networkInterface : QNetworkInterface::allInterfaces()) {
        const QString name = networkInterface.humanReadableName().toLower();
        const bool isWifi = name.contains("wi-fi") || name.contains("wifi")
            || name.contains("wlan") || name.contains(QStringLiteral("无线"));
        if (isWifi && isActiveInterface(networkInterface))
            return QStringLiteral("已连接");
    }
    return QStringLiteral("未连接");
}

QString TerminalInfoProvider::lockPassword()
{
    return {};
}

QString TerminalInfoProvider::osModel()
{
#ifdef Q_OS_WIN
    return QStringLiteral("Windows");
#else
    return QSysInfo::productType();
#endif
}

QString TerminalInfoProvider::osVersion()
{
    return QSysInfo::prettyProductName();
}

QString TerminalInfoProvider::osSerialNumber()
{
    return registryValue(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"),
        QStringLiteral("ProductId"));
}

QString TerminalInfoProvider::osInstallTime()
{
    const QString raw = registryValue(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"),
        QStringLiteral("InstallDate"));
    bool ok = false;
    const qint64 seconds = raw.toLongLong(&ok);
    return ok
        ? QDateTime::fromSecsSinceEpoch(seconds).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QString{};
}

QString TerminalInfoProvider::multiOsStatus()
{
    return QStringLiteral("未知");
}

QString TerminalInfoProvider::defaultGatewayForInterface(const QString& interfaceName)
{
    Q_UNUSED(interfaceName);
    return {};
}

QString TerminalInfoProvider::dnsForInterface(const QString& interfaceName)
{
    Q_UNUSED(interfaceName);
    return {};
}

QString TerminalInfoProvider::networkNameForInterface(const QString& interfaceName)
{
    Q_UNUSED(interfaceName);
    return {};
}
