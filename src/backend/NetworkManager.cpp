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
