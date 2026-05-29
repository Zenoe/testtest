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

bool SessionManager::isVpnConnected() const {
    return m_session.vpnConnected && m_session.isValid();
}

QString SessionManager::token() const {
    return m_session.token;
}

QString SessionManager::username() const {
    return m_session.username;
}

QString SessionManager::vpnConfPath() const {
    return m_session.getVpnConf();
}
UserSession SessionManager::session() const {
    return m_session;
}

// ── Persistence ───────────────────────────────────────────────────────────────

void SessionManager::setVpnConf(const QString &endpoint, const QString &ifaddr, const QStringList &allowedIPs) {
    // Using std::move physically transfers the data from the parameters into the map,
    // resulting in ZERO allocations and ZERO deep copies.
    m_session.vpnConfMap[std::move(endpoint)] = qMakePair(std::move(ifaddr), std::move(allowedIPs));
}

std::optional<QPair<QString, QStringList>> SessionManager::getVpnConf(const QString& endpoint) const
{
    auto it = m_session.vpnConfMap.find(endpoint);
    if (it != m_session.vpnConfMap.end()) {
        return it.value();
    }
    return std::nullopt;
}

void SessionManager::setInstalledRow(MIB_IPFORWARD_ROW2 row) {
   // Store the installed row for later cleanup
	m_session.installedRow = row;
}

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
