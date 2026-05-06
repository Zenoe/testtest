#pragma once
#include <QString>
#include <QStandardPaths>
#include <QRandomGenerator>

struct UserSession {
	//QString serverHost; 
	QString username;
	QString token;
	bool rememberMe = false;

	QString vpnconf = "C:/Users/2004l/Downloads/client1.conf";// fixme
	bool vpnConnected = false;
	//QDateTime& expiry;
	bool isValid() const { return !token.isEmpty(); }
	void clear() { username.clear(); token.clear(); rememberMe = false; vpnConnected = false; }

	QString confPath = resolveConfPath();

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
		QString name = generateRandomString(16) + ".conf";
		return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/" + name;
	}
};
