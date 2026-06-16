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
#include <QStringList>
#include <QtMath>
#include <limits>
#include <optional>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <ws2ipdef.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
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

bool isVirtualOrTunnelInterface(const QNetworkInterface& networkInterface)
{
    const QString identity = QStringLiteral("%1 %2")
        .arg(networkInterface.name(), networkInterface.humanReadableName())
        .toLower();

    static const QStringList markers{
        "virtualbox",
        "virtual",
        "host-only",
        "vmware",
        "hyper-v",
        "vethernet",
        "docker",
        "wsl",
        "npcap",
        "loopback",
        "wireguard",
        "wintun",
        "tap",
        "tun",
        "tailscale",
        "zerotier",
        "bluetooth",
        "pseudo",
        "isatap",
        "teredo",
    };

    for (const auto& marker : markers) {
        if (identity.contains(marker))
            return true;
    }
    return false;
}

std::optional<quint32> defaultRouteInterfaceIndex()
{
#ifdef Q_OS_WIN
    MIB_IPFORWARD_TABLE2* rawTable = nullptr;
    const DWORD rc = GetIpForwardTable2(AF_INET, &rawTable);
    if (rc != NO_ERROR) {
        spdlog::warn("TerminalInfoProvider: GetIpForwardTable2(AF_INET) failed, error={}", rc);
        return std::nullopt;
    }

    struct TableGuard {
        MIB_IPFORWARD_TABLE2* table = nullptr;
        ~TableGuard() { if (table) FreeMibTable(table); }
    } guard{ rawTable };

    quint32 bestIndex = 0;
    ULONG bestMetric = std::numeric_limits<ULONG>::max();
    for (ULONG i = 0; i < rawTable->NumEntries; ++i) {
        const MIB_IPFORWARD_ROW2& row = rawTable->Table[i];
        if (row.DestinationPrefix.Prefix.si_family != AF_INET
            || row.DestinationPrefix.PrefixLength != 0
            || row.InterfaceIndex == 0) {
            continue;
        }

        if (row.Metric < bestMetric) {
            bestMetric = row.Metric;
            bestIndex = row.InterfaceIndex;
        }
    }

    if (bestIndex == 0)
        return std::nullopt;
    return bestIndex;
#else
    return std::nullopt;
#endif
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

QString firstUsableIpv4(const QNetworkInterface& networkInterface)
{
    for (const auto& entry : networkInterface.addressEntries()) {
        const QHostAddress address = entry.ip();
        if (address.protocol() != QAbstractSocket::IPv4Protocol || address.isLoopback())
            continue;

        const QString ip = address.toString();
        if (ip.startsWith(QStringLiteral("169.254.")) || ip == QStringLiteral("0.0.0.0"))
            continue;

        return ip;
    }
    return {};
}

bool isRealActiveNetworkInterface(const QNetworkInterface& networkInterface)
{
    return isActiveInterface(networkInterface)
        && !isVirtualOrTunnelInterface(networkInterface)
        && !firstUsableIpv4(networkInterface).isEmpty()
        && !networkInterface.hardwareAddress().isEmpty();
}

int interfacePriority(const QNetworkInterface& networkInterface)
{
    const QString name = QStringLiteral("%1 %2")
        .arg(networkInterface.name(), networkInterface.humanReadableName())
        .toLower();

    if (name.contains("ethernet") || name.contains(QStringLiteral("以太网")))
        return 30;
    if (name.contains("wi-fi") || name.contains("wifi")
        || name.contains("wlan") || name.contains(QStringLiteral("无线")))
        return 20;
    return 10;
}

std::optional<QNetworkInterface> preferredRealNetworkInterface()
{
    const std::optional<quint32> defaultIndex = defaultRouteInterfaceIndex();
    std::optional<QNetworkInterface> best;
    int bestPriority = -1;

    for (const auto& networkInterface : QNetworkInterface::allInterfaces()) {
        if (!isRealActiveNetworkInterface(networkInterface))
            continue;

        if (defaultIndex.has_value()
            && static_cast<quint32>(networkInterface.index()) == defaultIndex.value()) {
            spdlog::debug("TerminalInfoProvider: selected default-route interface='{}'",
                networkInterface.humanReadableName().toStdString());
            return networkInterface;
        }

        const int priority = interfacePriority(networkInterface);
        if (!best.has_value() || priority > bestPriority) {
            best = networkInterface;
            bestPriority = priority;
        }
    }

    return best;
}

} // namespace

QString TerminalInfoSnapshot::hardwareCode() const
{
    return payload.value("pcInfo").toObject().value("hardwareCode").toString();
}

TerminalInfoSnapshot TerminalInfoProvider::collect()
{
    TerminalInfoSnapshot snapshot;
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
    QStringList stableParts;

    const QByteArray machineId = QSysInfo::machineUniqueId();
    if (!machineId.isEmpty())
        stableParts << QString::fromLatin1(machineId.toHex());
    else {
		stableParts << serialNumber()
			<< manufacturer()
			<< model()
			<< motherboard()
			<< cpuModel();
	}

    stableParts.removeAll(QString{});
    if (stableParts.isEmpty()) {
        spdlog::error("TerminalInfoProvider: no stable hardware identity is available");
        return {};
    }

    const QByteArray source = stableParts.join('|').toUtf8();
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
            { "ipv4", firstUsableIpv4(networkInterface) },
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
    const auto selectedInterface = preferredRealNetworkInterface();
    if (selectedInterface.has_value()) {
        const QString ip = firstUsableIpv4(selectedInterface.value());
        spdlog::debug("TerminalInfoProvider: selected terminalIp='{}' from interface='{}'",
            ip.toStdString(),
            selectedInterface->humanReadableName().toStdString());
        return ip;
    }

    spdlog::warn("TerminalInfoProvider: no real active network interface with IPv4 found");
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
    const auto selectedInterface = preferredRealNetworkInterface();
    return selectedInterface.has_value() ? selectedInterface->hardwareAddress() : QString{};
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
