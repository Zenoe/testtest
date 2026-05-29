#pragma once
#include <QObject>
#include "models/UserSession.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <ws2ipdef.h>
class SessionManager : public QObject {
    Q_OBJECT

public:
    static SessionManager& instance();

    void setSession(const UserSession& session);
    void clearSession();

    bool          isLoggedIn()  const;
    bool          rememberMe() const;
  bool isVpnConnected() const;
    QString       token()       const;
    QString       username()    const;
	QString       vpnConfPath()  const;
    UserSession   session()     const;

	void setVpnConf(const QString& endpoint, const QString& ifaddr, const  QStringList& allowedIPs);
	std::optional<QPair<QString, QStringList>> getVpnConf(const QString& endpoint) const;
  
  void setInstalledRow(MIB_IPFORWARD_ROW2);
signals:
    void sessionChanged();

private:
    explicit SessionManager(QObject* parent = nullptr);
    ~SessionManager() = default;

    SessionManager(const SessionManager&)            = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    void persistAutoLogin();    // writes token to QSettings if autoLogin is set
    void loadAutoLogin();       // reads on startup
    void clearPersistedAutoLogin();

    UserSession m_session;
};
