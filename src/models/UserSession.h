#pragma once
#include <QString>
#include <QMap>
#include <QStandardPaths>
#include <QRandomGenerator>

// for MIB_IPFORWARD_ROW2
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include "VpnConf.h"

struct UserSession {
	//QString serverHost; 
	QString username;
	QString token;
	bool rememberMe = false;

	bool vpnConnected = false;
	//QDateTime& expiry;

	MIB_IPFORWARD_ROW2 installedRow;
	QMap<QString, QPair<QString, QStringList>> vpnConfMap;
	/* QMap<QString,{QString, QStringList} > vpnConfMap; */
	bool isValid() const { return !token.isEmpty(); }
	void clear() { username.clear(); token.clear(); rememberMe = false; vpnConnected = false; confPath.clear();}
	QString getVpnConf() const{
    if (confPath.isEmpty()) {
        confPath = resolveConfPath();
    }
    return confPath;
	}

	UserSession() { }

UserSession(const QString& name, const QString& token,  bool remember)
  : username(name),  token(token), rememberMe(remember) {}


private:
  QString mutable confPath;
private:
  static inline QString generateRandomString(int length = 16) {
    const QString chars = QStringLiteral("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    QString result;
    result.reserve(length);
    for (int i = 0; i < length; ++i)
      result += chars[QRandomGenerator::global()->bounded(chars.size())];
    return result;
  }

  static inline QString resolveConfPath() {
    //QSettings settings;
    //QString name = settings.value("confFileName").toString();
    //if (name.isEmpty()) {
    //name = generateRandomString(16) + ".conf";
    //settings.setValue("confFileName", name);
    //}
    //return  "C:/Users/2004l/Downloads/clientxt.conf";

    QString name = "XY" + generateRandomString(16) + ".conf";
    /* name = "XY123.conf"; */
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/" + name;
  }
};
