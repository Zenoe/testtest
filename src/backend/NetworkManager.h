#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include "models/ServerConfig.h"
#include "models/AppEntry.h"
#include "models/UserSession.h"

class NetworkManager : public QObject {
    Q_OBJECT

public:
    static NetworkManager& instance();

    void setServer(const ServerConfig& cfg);
    const ServerConfig& serverConfig() const { return m_server; }

    // All calls are async; results arrive via signals
    void testConnectivity();
    void login(const QString& username, const QString& password);
    void fetchAppList(const QString& token);
    void logout(const QString& token);          // optional server-side invalidation
    void fetchCaptcha(const QString& url);
    //void fetchCaptcha(const QString& url, std::function<void(bool success, const QString& imgBase64, const QString& uuid, QString errStr)> callback);

	void get(const QUrl& url, std::function<void(QNetworkReply*)> callback);
signals:
    void connectivityResult(bool ok, const QString& message);
    void loginResult(bool ok, const UserSession& session, const QString& errorMsg);
    void appListReady(const QList<AppEntry>& apps);
    void logoutFinished();
    void networkError(const QString& message);
    void captchaFetched(bool success,
                        const QString& imgBase64,
                        const QString& uuid,
                        bool captchaEnabled,
                        const QString& errorMsg);
private:
    explicit NetworkManager(QObject* parent = nullptr);
    ~NetworkManager() = default;

    NetworkManager(const NetworkManager&)            = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    QUrl baseUrl(const QString& path) const;
    void handleReplyError(QNetworkReply* reply, const QString& context);

    QNetworkAccessManager* m_nam;
    ServerConfig           m_server;
};
