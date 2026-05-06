#pragma once
#include <QObject>
#include "models/UserSession.h"

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
	QString       vpnConf()  const;
    UserSession   session()     const;

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
