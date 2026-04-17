#pragma once
#include <QString>
#include <QJsonObject>
#include <nlohmann/json.hpp>

struct ServerConfig {
    QString  host;
    quint16  port  = 8080;
    QString  group;              // optional — user-defined grouping label

    bool isValid() const { return !host.trimmed().isEmpty() && port > 0; }

    //QJsonObject toJson() const {
    //    return {
    //        { "host",  host  },
    //        { "port",  port  },
    //        { "group", group }
    //    };
    //}

    nlohmann::json toJson() const {
        return {
            { "host",  host.toStdString()  }, // Convert QString to std::string
            { "port",  port                }, // quint16 is an integer, this works fine
            { "group", group.toStdString() }  // Convert QString to std::string
        };
    }
    // Helper to load from a json object
    static ServerConfig fromJson(const nlohmann::json& j) {
        ServerConfig cfg;
        // Use .value() to provide defaults if keys are missing
        cfg.host = QString::fromStdString(j.value("host", "127.0.0.1"));
        cfg.port = j.value("port", (quint16)8080);
        cfg.group = QString::fromStdString(j.value("group", "default"));
        return cfg;
    }

    //static ServerConfig fromJson(const QJsonObject& o) {
    //    return {
    //        o["host"].toString(),
    //        static_cast<quint16>(o["port"].toInt(8080)),
    //        o["group"].toString()
    //    };
    //}
};
