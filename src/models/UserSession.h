#pragma once
#include <QString>

struct UserSession {
    QString username;
    QString token;
    bool    autoLogin = false;

    bool isValid() const { return !token.isEmpty(); }
    void clear()         { username.clear(); token.clear(); autoLogin = false; }
};
