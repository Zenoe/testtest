#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include "models/ServerConfig.h"
#include "models/AppEntry.h"
#include "models/UserSession.h"
#include "models/VpnConf.h"

enum class ParseResult { Ok, NetworkError, Unauthorized, ServerError };

class NetworkManager : public QObject {
    Q_OBJECT

public:
    static NetworkManager& instance();

    void setServer(const ServerConfig& cfg);
    //const ServerConfig& serverConfig() const { return m_server; }

    // All calls are async; results arrive via signals
    void testConnectivity();
    void fetchAppList(const QString& token);

    void fetchSandBoxConf();
    void logout();          // optional server-side invalidation
    void fetchCaptcha();
    void fetchVpnConf();
    //void fetchCaptcha(const QString& url, std::function<void(bool success, const QString& imgBase64, const QString& uuid, QString errStr)> callback);

	void get(const QUrl& url, std::function<void(QNetworkReply*)> callback);
    void login(const QString& username, const QString& password, const QString& code, const QString& uuid);
    QNetworkRequest createRequest(const QString& url) const;
    ParseResult parseStandardReply(QNetworkReply* reply, QJsonObject& outObj, QString& errorMsg);
    void handleUnauthorized();

signals:
    void connectivityResult(bool ok, const QString& message);
	void serverError(const QString& message) const;
    void unauthorized();

  void loginFinished(bool success, const QString& token, const QString& errorMsg);
    void appListReady(const QList<AppEntry>& apps);
    void logoutFinished();
    void networkError(const QString& message);
    void captchaFetched(bool success,
                        const QString& imgBase64,
                        const QString& uuid,
                        bool captchaEnabled,
                        const QString& errorMsg);
    void vpnConfFetched(bool success, const VpnConfig& config, const QString& errorMsg);
private:
    explicit NetworkManager(QObject* parent = nullptr);
    ~NetworkManager() = default;

    NetworkManager(const NetworkManager&)            = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    QUrl baseUrl(const QString& path) const;
    void handleReplyError(QNetworkReply* reply, const QString& context);

    QTimer* startTimeoutTimer(QNetworkReply* reply, const QString& context);

    QNetworkAccessManager* m_nam;
    //ServerConfig           m_server;
};
