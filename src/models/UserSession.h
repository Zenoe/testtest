#pragma once
#include <QString>
#include <QStandardPaths>
#include <QRandomGenerator>

struct UserSession {
	//QString serverHost; 
	QString username;
	QString token;
	bool rememberMe = false;

	bool vpnConnected = false;
	//QDateTime& expiry;
	bool isValid() const { return !token.isEmpty(); }
	void clear() { username.clear(); token.clear(); rememberMe = false; vpnConnected = false; confPath.clear();}
	QString getVpnConf() const{

    if (confPath.isEmpty()) {
        confPath = resolveConfPath();
    }
	return confPath;
	}

  UserSession(){
  }

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
		//	name = generateRandomString(16) + ".conf";
		//	settings.setValue("confFileName", name);
		//}
    return  "C:/Users/2004l/Downloads/client1.conf";

		QString name = generateRandomString(16) + ".conf";
		return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/" + name;
	}
};
