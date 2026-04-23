#pragma once
#include <QString>

struct UserSession {
    QString username;
    QString token;
	bool rememberMe = false;   // whether the user checked "remember me" at login
    //QDateTime& expiry;
    bool isValid() const { return !token.isEmpty(); }
    void clear()         { username.clear(); token.clear(); rememberMe = false; }
};
