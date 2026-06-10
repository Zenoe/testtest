#pragma once

#include <QObject>
#include <QString>
#include <QThreadPool>

class VpnManager : public QObject {
    Q_OBJECT

public:
    static VpnManager& instance();

    void initializeControlService();
    void connectVpn(const QString& endpoint, const QString& confPath);
    void disconnectVpn(const QString& confPath);

signals:
    void connected(const QString& endpoint);
    void disconnected(const QString& serviceName);
    void errorOccurred(const QString& context, const QString& detail);

private:
    explicit VpnManager(QObject* parent = nullptr);
    ~VpnManager() override;
    VpnManager(const VpnManager&) = delete;
    VpnManager& operator=(const VpnManager&) = delete;

    bool ensureControlService();
    static QString serviceExePath();
    static QString serviceNameFrom(const QString& confPath);

    QThreadPool m_worker;
};
