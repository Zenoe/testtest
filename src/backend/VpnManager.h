#pragma once
#include <QObject>
#include <QString>
#include "ExitCodes.h"

using EC = XyreExitCode::Code;

// Replace the bool return with a richer result
struct CmdResult {
    EC      code;
    QString output;

    bool ok()   const { return code == EC::Ok; }
    bool soft() const { return XyreExitCode::isSoftCode(code); }
};

class VpnManager : public QObject {
    Q_OBJECT

public:
    static VpnManager& instance();

    void connectVpn(const QString& confPath);
    void disconnectVpn(const QString& confPath);

signals:
    void connected(const QString& serviceName);
    void disconnected(const QString& serviceName);
    void errorOccurred(const QString& context, const QString& detail);

private:
    explicit VpnManager(QObject* parent = nullptr);
    ~VpnManager() = default;
    VpnManager(const VpnManager&)            = delete;
    VpnManager& operator=(const VpnManager&) = delete;

    // Returns true on success; emits errorOccurred on failure
    CmdResult runServiceCommand(const QString& serviceExe,
                           const QStringList& args,
                           const QString& context,
                           int timeoutMs = 8'000);

    static QString serviceExePath();
    static QString serviceNameFrom(const QString& confPath);
};
