#pragma once
#include <QString>

struct UserSession {
    //QString serverHost; 
    QString username;
    QString token;
	bool rememberMe = false;
	QString vpnconf = "C:/Users/2004l/Downloads/client1.conf";// fixme
    //QDateTime& expiry;
    bool isValid() const { return !token.isEmpty(); }
    void clear()         { username.clear(); token.clear(); rememberMe = false; }
};
