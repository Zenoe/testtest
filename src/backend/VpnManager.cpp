#include "VpnManager.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>

#include "utils/logger.h"

VpnManager& VpnManager::instance() {
    static VpnManager inst;
    return inst;
}

VpnManager::VpnManager(QObject* parent) : QObject(parent) {}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString VpnManager::serviceExePath() {
    return QCoreApplication::applicationDirPath() + "/xyreService.exe";
}

QString VpnManager::serviceNameFrom(const QString& confPath) {
    return "XyGuardTunnel$" + QFileInfo(confPath).baseName();
}

CmdResult VpnManager::runServiceCommand(const QString& serviceExe,
                                        const QStringList& args,
                                        const QString& context,
                                        int timeoutMs)
{
    spdlog::info("[VPN] {} | {} {}", context.toStdString(),
                 serviceExe.toStdString(), args.join(' ').toStdString());

    QProcess proc;
    proc.setProgram(serviceExe);
    proc.setArguments(args);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start();

    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        const QString msg = "timed out after " + QString::number(timeoutMs) + "ms";
        spdlog::warn("[VPN] {} | {}", context.toStdString(), msg.toStdString());
        emit errorOccurred(context, msg);
        return { EC::UnexpectedError, msg };
    }

    const QString output = QString::fromUtf8(proc.readAll()).trimmed();
    const EC code        = static_cast<EC>(proc.exitCode());

    if (!output.isEmpty())
        spdlog::info("[VPN] {} | output: {}", context.toStdString(), output.toStdString());

    spdlog::info("[VPN] {} | exit code: {}", context.toStdString(), static_cast<int>(code));

    if (!XyreExitCode::isSoftCode(code)) {
        emit errorOccurred(context + QString::number(code), output);
    }

    return { code, output };
}

void VpnManager::connectVpn(const QString& confPath) {
    const QString exe         = serviceExePath();
    const QString serviceName = serviceNameFrom(confPath);

    spdlog::info("[VPN] connectVpn | config={} service={}",
                 confPath.toStdString(), serviceName.toStdString());

    const auto addResult = runServiceCommand(exe, { "add", confPath }, "add");
    if (!addResult.soft()) return;  // hard failure — errorOccurred already emitted

    if (addResult.code == EC::AlreadyExists)
        spdlog::info("[VPN] connectVpn | service already registered, skipping add");

    const auto startResult = runServiceCommand(exe, { "start", serviceName }, "start");
    if (!startResult.soft()) return;

    if (startResult.code == EC::AlreadyRunning)
        spdlog::info("[VPN] connectVpn | tunnel was already running");

    spdlog::info("[VPN] connectVpn | tunnel up: {}", serviceName.toStdString());
    emit connected(serviceName);
}

void VpnManager::disconnectVpn(const QString& confPath) {
    const QString exe         = serviceExePath();
    const QString serviceName = serviceNameFrom(confPath);

    spdlog::info("[VPN] disconnectVpn | service={}", serviceName.toStdString());

    const auto stopResult   = runServiceCommand(exe, { "stop",      serviceName }, "stop");
    const auto removeResult = runServiceCommand(exe, { "uninstall", serviceName }, "uninstall");

    // AlreadyStopped / NotFound on stop are fine
    // NotFound on uninstall means it's already gone — also fine
    const bool stopOk   = stopResult.soft();
    const bool removeOk = removeResult.soft();

    if (stopOk && removeOk) {
        spdlog::info("[VPN] disconnectVpn | tunnel down: {}", serviceName.toStdString());
        emit disconnected(serviceName);
    }
}
