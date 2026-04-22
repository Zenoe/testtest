// backend/NetworkManager.cpp  (key methods)
#include "NetworkManager.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

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

void NetworkManager::login(const QString& user, const QString& pass) {
    QJsonObject body{{ "username", user }, { "password", pass }};
    QNetworkRequest req(baseUrl("/api/auth/login"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto* reply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (reply->error() != QNetworkReply::NoError) {
            emit loginResult(false, {}, reply->errorString());
            reply->deleteLater();
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll()).object();
        UserSession s;
        s.token    = doc["token"].toString();
        s.username = doc["username"].toString();
        emit loginResult(!s.token.isEmpty(), s,
                         s.token.isEmpty() ? "Invalid credentials" : "");
        reply->deleteLater();
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
        emit captchaFetched(false, {}, {}, false, "URL未配置");
        return;
    }

    get(url, [this](QNetworkReply* reply) {
        reply->deleteLater();   // 防止内存泄漏

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

        QString imgBase64 = obj.value("img").toString();
        QString uuid = obj.value("uuid").toString();
        bool    captchaEnabled = obj.value("captchaEnabled").toBool(false);

        emit captchaFetched(true, imgBase64, uuid, captchaEnabled, "");
        });
}

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
