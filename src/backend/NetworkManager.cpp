// backend/NetworkManager.cpp  (key methods)
#include "NetworkManager.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

NetworkManager& NetworkManager::instance() {
    static NetworkManager inst;
    return inst;
}
NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    // Abort all pending requests if the network goes down
    connect(m_nam, &QNetworkAccessManager::finished,
            this, [](QNetworkReply* reply) {
                reply->deleteLater();
            });
}

QUrl NetworkManager::baseUrl(const QString& path) const {
    return QUrl(QString("http://%1:%2%3")
                .arg(m_server.host)
                .arg(m_server.port)
                .arg(path));
}

void NetworkManager::handleReplyError(QNetworkReply* reply,
                                      const QString& context) {
    const QString msg = QString("[%1] %2").arg(context, reply->errorString());
    emit networkError(msg);
}

void NetworkManager::setServer(const ServerConfig& cfg) {
    m_server = cfg;
}

void NetworkManager::testConnectivity() {
    auto* reply = m_nam->get(QNetworkRequest(baseUrl("/api/ping")));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        bool ok = (reply->error() == QNetworkReply::NoError);
        emit connectivityResult(ok, ok ? "OK" : reply->errorString());
        reply->deleteLater();
    });
}

void NetworkManager::login(const QString& url, const QString& username,
                           const QString& password, const QString& code, const QString& uuid) {
    if (url.isEmpty()) {
        emit loginFinished(false, "", "登录URL为空");
        return;
    }

    QUrl qurl(url);
    QNetworkRequest request(qurl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 构造 JSON
    QJsonObject json;
    json["username"] = username;
    json["password"] = password;
    json["code"]     = code;
    json["uuid"]     = uuid;

    QByteArray postData = QJsonDocument(json).toJson();

    QNetworkReply* reply = m_nam->post(request, postData);

    // 超时控制（复用你 captcha 的写法）
    QTimer* timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->start(10000);

    connect(timeoutTimer, &QTimer::timeout, this, [this, reply, timeoutTimer]() {
        if (reply && reply->isRunning()) {
            reply->abort();
            emit loginFinished(false, {}, "登录请求超时（10秒）");
        }
        timeoutTimer->deleteLater();
        if (reply) reply->deleteLater();
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, timeoutTimer]() {
        timeoutTimer->stop();
        timeoutTimer->deleteLater();
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit loginFinished(false, {}, reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);

        if (!doc.isObject()) {
            emit loginFinished(false, {}, "返回数据格式错误");
            return;
        }

        QJsonObject obj = doc.object();
        int code = obj.value("code").toInt();

        if (code != 200) {
            QString msg = obj.value("msg").toString("登录失败");
            emit loginFinished(false, {}, msg);
            return;
        }

        QString token = obj.value("token").toString();

        emit loginFinished(true, token, "");
    });
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

void NetworkManager::logout(const QString& token) {
    QNetworkRequest req(baseUrl("/api/auth/logout"));
    req.setRawHeader("Authorization",
                     QByteArray("Bearer ") + token.toUtf8());
    auto* reply = m_nam->post(req, QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        emit logoutFinished();
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

void NetworkManager::fetchCaptcha(const QString& url)
{
    if (url.isEmpty()) {
        emit captchaFetched(false, {}, {}, false, "验证码URL配置为空");
        return;
    }
    //QUrl qurl(url);
    //QNetworkRequest request(qurl);
    //QNetworkReply* reply = m_nam->get(request);
    QNetworkReply* reply = m_nam->get(QNetworkRequest(QUrl(url)));
    //QNetworkRequest request(QUrl(url));
    //QNetworkReply* reply = m_nam->get(request);

    QTimer* timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->start(10000);   // 10秒超时，可自行修改

    // 超时处理
    connect(timeoutTimer, &QTimer::timeout, this, [this, reply, timeoutTimer]() {
        if (reply && reply->isRunning()) {
            reply->abort();
            emit captchaFetched(false, {}, {}, false, "请求验证码超时（10秒）");
        }
        timeoutTimer->deleteLater();
        if (reply) reply->deleteLater();
    });

    // 正常完成
    connect(reply, &QNetworkReply::finished, this, [this, reply, timeoutTimer]() {
        timeoutTimer->stop();
        timeoutTimer->deleteLater();

        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit captchaFetched(false, {}, {}, false, reply->errorString());
            return;
        }

        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);

        if (doc.isNull() || !doc.isObject()) {
            emit captchaFetched(false, {}, {}, false, "返回数据不是有效的JSON");
            return;
        }

        QJsonObject obj = doc.object();
        int code = obj.value("code").toInt();

        if (code != 200) {
            QString msg = obj.value("msg").toString("未知错误");
            emit captchaFetched(false, {}, {}, false, msg);
            return;
        }

        QString imgBase64     = obj.value("img").toString();
        QString uuid          = obj.value("uuid").toString();
        bool    captchaEnabled = obj.value("captchaEnabled").toBool(false);

        emit captchaFetched(true, imgBase64, uuid, captchaEnabled, "");
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
