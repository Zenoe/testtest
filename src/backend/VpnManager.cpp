#include "VpnManager.h"

#include "ControlServiceClient.h"
#include "utils/logger.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>

VpnManager& VpnManager::instance() {
    static VpnManager inst;
    return inst;
}

VpnManager::VpnManager(QObject* parent) : QObject(parent) {
    m_worker.setMaxThreadCount(1);
    m_worker.setExpiryTimeout(-1);
}

VpnManager::~VpnManager() {
    m_worker.waitForDone();
}

QString VpnManager::serviceExePath() {
    return QCoreApplication::applicationDirPath() + "/xyreService.exe";
}

QString VpnManager::serviceNameFrom(const QString& confPath) {
    return "XyGuardTunnel$" + QFileInfo(confPath).baseName();
}

bool VpnManager::ensureControlService() {
    const ControlResponse response =
        ControlServiceClient::ensureAvailable(serviceExePath());
    if (response.ok)
        return true;

    spdlog::error("[VPN] controller unavailable | code={} win32={} detail={}",
                  response.code, response.win32, response.detail.toStdString());
    emit errorOccurred(QStringLiteral("controller"), response.detail);
    return false;
}

void VpnManager::initializeControlService() {
    m_worker.start([this] {
        ensureControlService();
    });
}

void VpnManager::connectVpn(const QString& endpoint, const QString& confPath) {
    const QString serviceName = serviceNameFrom(confPath);
    const QFileInfo confInfo(confPath);

    spdlog::info("[VPN] connectVpn | config={} service={}",
                 confPath.toStdString(), serviceName.toStdString());

    if (!confInfo.exists() || !confInfo.isFile() || !confInfo.isReadable()) {
        const QString detail =
            QStringLiteral("WireGuard config does not exist or is not readable: %1")
                .arg(confPath);
        spdlog::error("[VPN] connectVpn | {}", detail.toStdString());
        emit errorOccurred(QStringLiteral("validate config"), detail);
        return;
    }

    const QString absolutePath = confInfo.absoluteFilePath();
    m_worker.start(
        [this, endpoint, confPath, serviceName, absolutePath] {
            if (!ensureControlService())
                return;

            const ControlResponse response = ControlServiceClient::send(QJsonObject{
                { QStringLiteral("command"), QStringLiteral("connect") },
                { QStringLiteral("configPath"), absolutePath }
            });
            if (!response.ok) {
                spdlog::error("[VPN] connect failed | code={} win32={} detail={}",
                              response.code, response.win32, response.detail.toStdString());
                emit errorOccurred(QStringLiteral("connect"), response.detail);
                return;
            }

            spdlog::info("[VPN] connectVpn | tunnel up: {}", serviceName.toStdString());
            if (!QFile::remove(confPath))
                spdlog::warn("[VPN] failed to delete: {}", confPath.toStdString());
            emit connected(endpoint);
        });
}

void VpnManager::disconnectVpn(const QString& confPath) {
    const QString serviceName = serviceNameFrom(confPath);
    spdlog::info("[VPN] disconnectVpn | service={}", serviceName.toStdString());

    m_worker.start([this, serviceName] {
        if (!ensureControlService())
            return;

        const ControlResponse response = ControlServiceClient::send(QJsonObject{
            { QStringLiteral("command"), QStringLiteral("disconnect") },
            { QStringLiteral("serviceName"), serviceName }
        });
        if (!response.ok) {
            spdlog::error("[VPN] disconnect failed | code={} win32={} detail={}",
                          response.code, response.win32, response.detail.toStdString());
            emit errorOccurred(QStringLiteral("disconnect"), response.detail);
            return;
        }

        spdlog::info("[VPN] disconnectVpn | tunnel down: {}", serviceName.toStdString());
        emit disconnected(serviceName);
    });
}
