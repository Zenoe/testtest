#include "SessionManager.h"
#include <QSettings>

// QSettings key constants — centralised to avoid typo-driven bugs
static constexpr char kGroup[]     = "AutoLogin";
static constexpr char kToken[]     = "token";
static constexpr char kUsername[]  = "username";
static constexpr char kAutoLogin[] = "autoLogin";

SessionManager& SessionManager::instance() {
    static SessionManager inst;
    return inst;
}

SessionManager::SessionManager(QObject* parent)
    : QObject(parent)
{
    loadAutoLogin();
}

// ── Public API ────────────────────────────────────────────────────────────────

void SessionManager::setSession(const UserSession& session) {
    m_session = session;
    if (session.rememberMe)
        persistAutoLogin();
    else
        clearPersistedAutoLogin();
    emit sessionChanged();
}

void SessionManager::clearSession() {
    m_session.clear();
    clearPersistedAutoLogin();
    emit sessionChanged();
}

bool SessionManager::isLoggedIn() const {
    return m_session.isValid();
}

bool SessionManager::rememberMe() const {
    return m_session.rememberMe && m_session.isValid();
}

QString SessionManager::token() const {
    return m_session.token;
}

QString SessionManager::username() const {
    return m_session.username;
}

QString SessionManager::vpnConf() const {
    return m_session.vpnconf;
}
UserSession SessionManager::session() const {
    return m_session;
}

// ── Persistence ───────────────────────────────────────────────────────────────

void SessionManager::persistAutoLogin() {
    QSettings s;
    s.beginGroup(kGroup);
    s.setValue(kToken,     m_session.token);
    s.setValue(kUsername,  m_session.username);
    s.setValue(kAutoLogin, true);
    s.endGroup();
    s.sync();
}

void SessionManager::clearPersistedAutoLogin() {
    QSettings s;
    s.beginGroup(kGroup);
    s.remove("");           // removes all keys inside this group
    s.endGroup();
    s.sync();
}

void SessionManager::loadAutoLogin() {
    QSettings s;
    s.beginGroup(kGroup);
    const bool autoLogin = s.value(kAutoLogin, false).toBool();
    if (autoLogin) {
        m_session.token     = s.value(kToken).toString();
        m_session.username  = s.value(kUsername).toString();
        m_session.rememberMe = true;
    }
    s.endGroup();
}
