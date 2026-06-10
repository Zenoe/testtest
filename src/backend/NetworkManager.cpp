#include "NetworkManager.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QStandardPaths>

#include "SessionManager.h"
#include <utils/ConfigManager.h>
#include "utils/logger.h"
#include <utils/RouteConflictChecker.h>

static constexpr int kTimeoutMs = 10'000;
static constexpr int kHttpOk = 200;
static constexpr int kHttpUnauthorized = 401;

/// helper: Safely stop + schedule deletion of a QTimer from any lambda context.
static void stopAndDelete(QTimer* t)
{
    if (!t) return;
    t->stop();
    t->deleteLater();
}

NetworkManager& NetworkManager::instance() {
    static NetworkManager inst;
    return inst;
}
NetworkManager::NetworkManager(QObject* parent) : QObject(parent), m_nam(new QNetworkAccessManager(this)), m_spa(this)
{
    if (!m_spa.loadFromConfig()) {
        spdlog::warn("NetworkManager: SPA config invalid — "
            "connections will proceed without port knock");
    }
    // Abort all pending requests if the network goes down
    connect(m_nam, &QNetworkAccessManager::finished,
            this, [](QNetworkReply* reply) {
                reply->deleteLater();
            });
}

QUrl NetworkManager::baseUrl(const QString& path) const {
    auto& cm = ConfigManager::instance();
    QString host = QString::fromStdString(
        cm.get<std::string>("server.host").value_or("")
    );
    if (host.isEmpty()) {
        emit serverError("服务器地址未配置");
        return {};
    }
    int port = cm.get<int>("server.port", 0);

    if (!host.startsWith("http://") && !host.startsWith("https://")) {
        host = "http://" + host;
    }

    QUrl url(host);
    if (port != 0) {
        url.setPort(port);
    }
    url.setPath(path);
    return url;
}

void NetworkManager::handleReplyError(QNetworkReply* reply,
	const QString& context) {
    const QString msg = QString("[%1] %2").arg(context, reply->errorString());
    emit networkError(msg);
}

QTimer* NetworkManager::startTimeoutTimer(QNetworkReply* reply, const QString& context)
{
    auto* timer = new QTimer(this);
    timer->setSingleShot(true);

    connect(timer, &QTimer::timeout, this, [reply, timer, context]() {
        spdlog::warn("{}: timed out after {}ms", context.toStdString(), kTimeoutMs);
        stopAndDelete(timer);
        if (reply && reply->isRunning())
            reply->abort(); // triggers finished() with AbortedError
        });

    timer->start(kTimeoutMs);
    return timer;
}

void NetworkManager::setServer(const ServerConfig& cfg) {
    //m_server = cfg;
}

void NetworkManager::testConnectivity() {
    auto* reply = m_nam->get(QNetworkRequest(baseUrl("/api/ping")));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        bool ok = (reply->error() == QNetworkReply::NoError);
        emit connectivityResult(ok, ok ? "OK" : reply->errorString());
        reply->deleteLater();
    });
}

QNetworkRequest NetworkManager::createRequest(const QString& endpoint) const
{
    const QUrl url = baseUrl(endpoint);
    spdlog::debug("createRequest: {} -> {}", endpoint.toStdString(),
        url.toString().toStdString());

    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Attach bearer token when a session is active
    const QString token = SessionManager::instance().token();
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization",
            QStringLiteral("Bearer %1").arg(token).toUtf8());
        spdlog::debug("createRequest: bearer token attached");
    }

    return request;
}

ParseResult NetworkManager::parseStandardReply(QNetworkReply* reply,
    QJsonObject& outObj,
    QString& errorMsg)
{
    // ── 1. Network-level error ────────────────
    if (reply->error() != QNetworkReply::NoError) {
        errorMsg = reply->errorString();
        spdlog::warn("parseStandardReply: network error [{}] {}",
            static_cast<int>(reply->error()),
            errorMsg.toStdString());
        return ParseResult::NetworkError;
    }

    // ── 2. Parse body ─────────────────────────
    const QByteArray  raw = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);

    if (!doc.isObject()) {
        errorMsg = "返回数据格式错误";
        spdlog::error("parseStandardReply: malformed JSON body: {}",
            QString(raw).left(256).toStdString());
        return ParseResult::ServerError;
    }

    const QJsonObject obj = doc.object();
    const int         code = obj.value("code").toInt(-1);

    // ── 3. Application-level error ────────────
    if (code != kHttpOk) {
        errorMsg = obj.value("msg").toString("请求失败");
        spdlog::warn("parseStandardReply: server code={} msg={}",
            code, errorMsg.toStdString());

        if (code == kHttpUnauthorized) {
            spdlog::info("parseStandardReply: 401 – triggering session teardown");
            handleUnauthorized();
        }
        return ParseResult::Unauthorized;
    }

    spdlog::debug("parseStandardReply: success (code={})", code);
    outObj = obj;
    return ParseResult::Ok;
}

void NetworkManager::handleUnauthorized()
{
    spdlog::warn("handleUnauthorized: – clearing session");
    SessionManager::instance().clearSession();
    emit unauthorized();   // let the UI react (e.g. redirect to login)
}

void NetworkManager::login(const QString& username,
                           const QString& password, const QString& code, const QString& uuid) {

    const QString endpoint = QString::fromStdString(
        ConfigManager::instance()
        .get<std::string>("server.login_endpoint")
        .value_or("/revelation/user/login"));

    spdlog::info("login: attempt for user='{}' endpoint='{}'",
        username.toStdString(), endpoint.toStdString());

    // ── Build request ─────────────────────────
    const QNetworkRequest request = createRequest(endpoint);

    const QJsonObject body{
        { "username", username },
        { "password", password },
        { "code",     code     },
        { "uuid",     uuid     },
    };
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = m_nam->post(request, payload);

    // ── Timeout guard ─────────────────────────
	auto* timer = startTimeoutTimer(reply, "login");

    // ── Reply handler ─────────────────────────
    connect(reply, &QNetworkReply::finished, this,
        [this, reply, timer, username]() {
            stopAndDelete(timer);

            // Handle abort → timeout message
            if (reply->error() == QNetworkReply::OperationCanceledError) {
                spdlog::error("login: aborted (timeout) for user='{}'",
                    username.toStdString());
                emit loginFinished(false, {}, tr("登录请求超时（10秒）"));
                reply->deleteLater();
                return;
            }

            QJsonObject obj;
            QString     errstr;
            if (parseStandardReply(reply, obj, errstr) != ParseResult::Ok) {
                spdlog::error("login: failed for user='{}': {}",
                    username.toStdString(), errstr.toStdString());
                emit loginFinished(false, {}, errstr);
                reply->deleteLater();
                return;
            }

            const QString token = obj.value("token").toString();
            if (token.isEmpty()) {
                const QString msg = "服务器未返回令牌";
                spdlog::error("login: success response but token missing for user='{}'",
                    username.toStdString());
                emit loginFinished(false, {}, msg);
                reply->deleteLater();
                return;
            }

            spdlog::info("login: success for user='{}'", username.toStdString());
            emit loginFinished(true, token, {});
            reply->deleteLater();
        });

}

void NetworkManager::logout()
{
    static constexpr char kEndpoint[] = "/revelation/logout";
    spdlog::info("logout: sending logout request");

    const QNetworkRequest request = createRequest(kEndpoint);
    QNetworkReply* reply = m_nam->post(request, QByteArray{});

    connect(reply, &QNetworkReply::finished, this,
        [this, reply]() {
            QJsonObject obj;
            QString     err;

            if (parseStandardReply(reply, obj, err) == ParseResult::Ok) {
                spdlog::info("logout: server confirmed logout");
                SessionManager::instance().clearSession();
                emit logoutFinished();
            }
            else {
                // Treat any server-side failure as a soft error:
                // always clear the local session so the UI isn't stuck.
                spdlog::warn("logout: server returned error '{}' – "
                    "clearing session locally anyway", err.toStdString());
                SessionManager::instance().clearSession();
                emit logoutFinished();          // or emit a separate logoutFailed(err) ??
            }

            reply->deleteLater();
        });
}

void NetworkManager::editPassword(const QString& oldPassword, const QString& newPassword)
{
    static constexpr char kEndpoint[] = "/revelation/user/editPassword";

    if (!SessionManager::instance().isLoggedIn()) {
        emit passwordEditFinished(false, tr("请先登录后再修改密码"));
        return;
    }

    const QJsonObject body{
        { "oldPassword", oldPassword },
        { "newPassword", newPassword },
    };
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    spdlog::info("editPassword: sending request");
    QNetworkReply* reply = m_nam->post(createRequest(kEndpoint), payload);
    auto* timer = startTimeoutTimer(reply, "editPassword");

    connect(reply, &QNetworkReply::finished, this, [this, reply, timer]() {
        stopAndDelete(timer);

        if (reply->error() == QNetworkReply::OperationCanceledError) {
            spdlog::error("editPassword: aborted (timeout)");
            emit passwordEditFinished(false, tr("修改密码请求超时"));
            reply->deleteLater();
            return;
        }

        QJsonObject obj;
        QString error;
        if (parseStandardReply(reply, obj, error) != ParseResult::Ok) {
            spdlog::error("editPassword: failed: {}", error.toStdString());
            emit passwordEditFinished(false, error);
            reply->deleteLater();
            return;
        }

        const QString message = obj.value("msg").toString(tr("修改密码成功"));
        spdlog::info("editPassword: success");
        emit passwordEditFinished(true, message);
        reply->deleteLater();
    });
}

void NetworkManager::fetchSandBoxConf() {
    spdlog::info("fetchSandBoxConf");
}

void NetworkManager::fetchAppList(const QString& token) {
    QNetworkRequest req(baseUrl("/api/apps"));
    req.setRawHeader("Authorization",
                     QByteArray("Bearer ") + token.toUtf8());
    auto* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        QList<AppEntry> apps;
        if (reply->error() == QNetworkReply::NoError) {
            auto arr = QJsonDocument::fromJson(reply->readAll()).array();
            for (const auto& v : arr) {
                auto o = v.toObject();
                apps.append({ o["id"].toString(),
                              o["name"].toString(),
                              o["path"].toString(), {} });
            }
        }
        emit appListReady(apps);
        reply->deleteLater();
    });
}

void NetworkManager::get(const QUrl& url, std::function<void(QNetworkReply*)> callback) {
        QNetworkRequest request(url);
        // Add default headers here (e.g., User-Agent, Auth tokens)
        request.setRawHeader("Accept", "application/json");

        QNetworkReply* reply = m_nam->get(request);
        connect(reply, &QNetworkReply::finished, [reply, callback]() {
            callback(reply);
            reply->deleteLater();
        });
    }

void NetworkManager::fetchCaptcha()
{
    const QString endpoint = QString::fromStdString(
        ConfigManager::instance()
        .get<std::string>("server.captcha_endpoint")
        .value_or("/revelation/captchaImage"));

    spdlog::debug("fetchCaptcha: requesting endpoint='{}'", endpoint.toStdString());

    QNetworkReply* reply = m_nam->get(createRequest(endpoint));

    auto* timer = startTimeoutTimer(reply, "fetchCaptcha");
    connect(reply, &QNetworkReply::finished, this,
        [this, reply, timer]() {
            stopAndDelete(timer);

            // ── Timeout / abort ───────────────────────
            if (reply->error() == QNetworkReply::OperationCanceledError) {
                spdlog::error("fetchCaptcha: aborted (timeout)");
                emit captchaFetched(false, {}, {}, false, tr("请求验证码超时（10秒）"));
                reply->deleteLater();
                return;
            }

            // ── Network-level error ───────────────────
            if (reply->error() != QNetworkReply::NoError) {
                const QString err = reply->errorString();
                spdlog::error("fetchCaptcha: network error [{}] {}",
                    static_cast<int>(reply->error()),
                    err.toStdString());
                emit captchaFetched(false, {}, {}, false, err);
                reply->deleteLater();
                return;
            }

            // ── Parse JSON ────────────────────────────
            const QByteArray   raw = reply->readAll();
            const QJsonDocument doc = QJsonDocument::fromJson(raw);

            if (!doc.isObject()) {
                spdlog::error("fetchCaptcha: malformed JSON: {}",
                    QString(raw).left(256).toStdString());
                emit captchaFetched(false, {}, {}, false, tr("返回数据不是有效的JSON"));
                reply->deleteLater();
                return;
            }

            const QJsonObject obj = doc.object();
            const int         code = obj.value("code").toInt(-1);

            // ── Application-level error ───────────────
            if (code != kHttpOk) {
                const QString msg = obj.value("msg").toString("未知错误");
                spdlog::warn("fetchCaptcha: server code={} msg={}", code,
                    msg.toStdString());
                emit captchaFetched(false, {}, {}, false, msg);
                reply->deleteLater();
                return;
            }

            // ── Validate payload fields ───────────────
            const QString imgBase64 = obj.value("img").toString();
            const QString uuid = obj.value("uuid").toString();
            const bool    captchaEnabled = obj.value("captchaEnabled").toBool(false);

            if (imgBase64.isEmpty() || uuid.isEmpty()) {
                spdlog::error("fetchCaptcha: success code but missing img/uuid fields");
                emit captchaFetched(false, {}, {}, false, tr("验证码数据不完整"));
                reply->deleteLater();
                return;
            }

            spdlog::info("fetchCaptcha: success captchaEnabled={}", captchaEnabled);
            emit captchaFetched(true, imgBase64, uuid, captchaEnabled, {});
            reply->deleteLater();
        });

}

void NetworkManager::fetchVpnConf()
{
    const QString endpoint = QString::fromStdString(
        ConfigManager::instance()
        .get<std::string>("server.vpnconf_endpoint")
        .value_or("/revelation/user/vpnConfig")); // todo when return 500, login ui should reset loading state, and show server unavailable

    spdlog::debug("fetchVpnConf: requesting endpoint='{}'", endpoint.toStdString());

    QNetworkReply* reply = m_nam->post(createRequest(endpoint), QByteArray{});
    auto* timer = startTimeoutTimer(reply, "fetchVpnConf");

    connect(reply, &QNetworkReply::finished, this,
        [this, reply, timer]() {
            stopAndDelete(timer);

            // ── Timeout / abort ───────────────────────
            if (reply->error() == QNetworkReply::OperationCanceledError) {
                spdlog::error("fetchVpnConf: aborted (timeout)");
                emit vpnConfFetched(false, {}, tr("获取VPN配置超时"));
                reply->deleteLater();
                return;
            }

            QJsonObject obj;
            QString     err;
            if (parseStandardReply(reply, obj, err) == ParseResult::Unauthorized) {
                //spdlog::error("fetchVpnConf: {}", err.toStdString());
                //emit vpnConfFetched(false, {}, err);
                reply->deleteLater();
                return;
            }

            const QJsonObject data = obj.value("data").toObject();
            const QJsonObject peer = data.value("peer").toObject();
            const QJsonObject iface = data.value("interface").toObject();

            const QString peerEndpoint = peer.value("endpoint").toString();
            const QString peerPublicKey = peer.value("publicKey").toString();
            const QString presharedKey = peer.value("presharedKey").toString();
            const QString privateKey = iface.value("privateKey").toString();
            const QString address = iface.value("address").toString();
            const QString dns = iface.value("dns").toString();

            if (peerEndpoint.isEmpty() || peerPublicKey.isEmpty()
                || privateKey.isEmpty() || address.isEmpty()) {
                spdlog::error("fetchVpnConf: missing required fields in response");
                emit vpnConfFetched(false, {}, tr("VPN配置数据不完整"));
                reply->deleteLater();
                return;
            }

            VpnConfig config;
            config.peer.endpoint = peerEndpoint;
            config.peer.publicKey = peerPublicKey;
            config.peer.presharedKey = presharedKey;
            config.peer.persistentKeepalive = peer.value("persistentKeepalive").toInt(25);
            config.peer.allowedIPs = [&] {
                QStringList list;
                for (const auto& ip : peer.value("allowedIPs").toArray())
                    list << ip.toString();
                return list;
                }();
            config.iface.privateKey = privateKey;
            config.iface.address = address;
            config.iface.dns = dns;
            config.iface.mtu = iface.value("mtu").toInt(1420);

            const RouteCheckResult check = checkRouteConflicts(
                config.iface.address,
                config.peer.allowedIPs);

            if (check.hasConflicts()) {
                spdlog::error("[VPN] Aborting config apply due to {} route conflict(s)",
                    check.conflicts.size());
                for (const auto& c : check.conflicts) {
                    spdlog::error("[VPN]   existing route {} (gw {}) conflicts with {} [{}]",
                        c.existingNetwork.toStdString(),
                        c.existingGateway.toStdString(),
                        c.conflictingCidr.toStdString(),
                        c.source.toStdString());
                }
                // surface to UI / return error to caller
                emit vpnConfFetched(false, {}, tr("VPN地址和本地路由有冲突"));
                reply->deleteLater();
                return;
            }

            spdlog::info("fetchVpnConf: success address='{}' endpoint='{}'",
                address.toStdString(), peerEndpoint.toStdString());

            emit vpnConfFetched(true, config, {});
            reply->deleteLater();
        });
}

// void NetworkManager::fetchCaptcha(const QString& url)
// {
//     if (url.isEmpty()) {
//         emit captchaFetched(false, {}, {}, false, "URLÎ´ÅäÖÃ");
//         return;
//     }

//     get(url, [this](QNetworkReply* reply) {
//         reply->deleteLater();   // ·ÀÖ¹ÄÚ´æÐ¹Â©

//         if (reply->error() != QNetworkReply::NoError) {
//             emit captchaFetched(false, {}, {}, false, reply->errorString());
//             return;
//         }

//         QByteArray responseData = reply->readAll();
//         QJsonDocument doc = QJsonDocument::fromJson(responseData);

//         if (doc.isNull() || !doc.isObject()) {
//             emit captchaFetched(false, {}, {}, false, "·µ»ØÊý¾Ý²»ÊÇÓÐÐ§µÄJSON");
//             return;
//         }

//         QJsonObject obj = doc.object();
//         int code = obj.value("code").toInt();

//         if (code != 200) {
//             QString msg = obj.value("msg").toString("Î´Öª´íÎó");
//             emit captchaFetched(false, {}, {}, false, msg);
//             return;
//         }

//         QString imgBase64 = obj.value("img").toString();
//         QString uuid = obj.value("uuid").toString();
//         bool    captchaEnabled = obj.value("captchaEnabled").toBool(false);

//         emit captchaFetched(true, imgBase64, uuid, captchaEnabled, "");
//         });
// }

//void NetworkManager::fetchCaptcha(const QString& url, std::function<void(bool success, const QString& imgBase64, const QString& uuid, QString errStr)> callback) {
//
//        NetworkManager::instance().get(url, [callback](QNetworkReply* reply) {
//            if (reply->error() != QNetworkReply::NoError) {
//				callback(false, {}, {}, reply->errorString());
//                return;
//            }
//
//            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
//            QJsonObject obj = doc.object();
//            
//            if (obj.value("code").toInt() != 200) {
//				callback(false, {}, {}, obj.value("msg").toString("Unknown Error"));
//            } else {
//				callback(true, obj.value("img").toString(), obj.value("uuid").toString(), "");
//            }
//        });
//    }
