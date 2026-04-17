// ui/MainWindow.cpp
#include "MainWindow.h"
#include "utils/ConfigManager.h"
#include "backend/NetworkManager.h"
#include "backend/SessionManager.h"
#include <QHBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    setupConnections();
    determineInitialState();
}

void MainWindow::setupUi() {
    setMinimumSize(900, 600);
    setWindowTitle("Revelation");
    setMenuBar(nullptr);   // no menu bar

    auto* central = new QWidget(this);
    auto* hLayout = new QHBoxLayout(central);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(0);

    m_nav = new NavigationWidget(this);
    m_nav->setFixedWidth(64);

    m_stack      = new QStackedWidget(this);
    m_serverCfg  = new ServerConfigWidget(this);
    m_login      = new LoginWidget(this);
    m_appGrid    = new AppGridWidget(this);

    m_stack->addWidget(m_serverCfg);   // index 0
    m_stack->addWidget(m_login);       // index 1
    m_stack->addWidget(m_appGrid);     // index 2

    hLayout->addWidget(m_nav);
    hLayout->addWidget(m_stack);
    setCentralWidget(central);
}

void MainWindow::setupConnections() {
    // ServerConfigWidget signals
    connect(m_serverCfg, &ServerConfigWidget::configSaved,
            this, &MainWindow::onConfigSaved);
    connect(m_serverCfg, &ServerConfigWidget::testRequested,
            this, &MainWindow::onTestConnection);

    // LoginWidget signals
    connect(m_login, &LoginWidget::loginRequested,
            this, &MainWindow::onLoginRequested);
    connect(m_login, &LoginWidget::logoutRequested,
            this, &MainWindow::onLogout);

    // AppGridWidget
    connect(m_appGrid, &AppGridWidget::logoutRequested,
            this, &MainWindow::onLogout);

    // NetworkManager callbacks
    auto& net = NetworkManager::instance();
    connect(&net, &NetworkManager::connectivityResult,
            m_serverCfg, &ServerConfigWidget::onTestResult);
    connect(&net, &NetworkManager::loginResult,
            this, &MainWindow::onLoginResult);
    connect(&net, &NetworkManager::appListReady,
            m_appGrid, &AppGridWidget::onAppsReceived);

    // Navigation
    connect(m_nav, &NavigationWidget::itemSelected,
            this, &MainWindow::onNavItemSelected);
}

void MainWindow::determineInitialState() {
    auto& cfg = ConfigManager::instance();
    if (!cfg.contains("server")) {
        switchToPage(0);
    } else {
        auto serverCfg = cfg.getSection("server");
        NetworkManager::instance().setServer(ServerConfig::fromJson(serverCfg));
        // Check for saved auto-login token
        auto& session = SessionManager::instance();
        if (session.hasAutoLogin()) {
            switchToPage(2);
            NetworkManager::instance().fetchAppList(session.token());
        } else {
            switchToPage(1);
        }
    }
}

void MainWindow::switchToPage(int index) {
    m_stack->setCurrentIndex(index);
    m_nav->setVisible(index != 0);     // hide nav on server-config screen

    // Keep sidebar highlight consistent with the active page
    switch (index) {
        case 1: m_nav->setActiveItem("workspace"); break;  // login → pre-auth
        case 2: m_nav->setActiveItem("workspace"); break;  // app grid
        default: break;
    }

    // Propagate logged-in username to avatar when entering main app
    if (index == 2) {
        m_nav->setUsername(
            SessionManager::instance().username());
    }
}
// void MainWindow::switchToPage(int index) {
//     m_stack->setCurrentIndex(index);
//     // Navigation sidebar visibility: hide on config screen (not logged in)
//     m_nav->setVisible(index != 0);
// }

void MainWindow::onConfigSaved(const ServerConfig& cfg) {
    //ConfigManager::instance().saveConfig(cfg);
    ConfigManager::instance().set("server", cfg.toJson());
    NetworkManager::instance().setServer(cfg);
    switchToPage(1);
}

void MainWindow::onTestConnection() {
    NetworkManager::instance().testConnectivity();
}

void MainWindow::onLoginRequested(const QString& user, const QString& pass) {
    m_login->setLoading(true);
    NetworkManager::instance().login(user, pass);
}

void MainWindow::onLoginResult(bool ok, const UserSession& session,
                               const QString& err) {
    m_login->setLoading(false);
    if (!ok) { m_login->showError(err); return; }
    SessionManager::instance().setSession(session);
    switchToPage(2);
    NetworkManager::instance().fetchAppList(session.token);
}

void MainWindow::onLogout() {
    SessionManager::instance().clearSession();
    switchToPage(1);
}

void MainWindow::onNavItemSelected(const QString& id) {
    // Map nav item IDs to stacked widget pages.
    // "workspace" is the primary page — it shows the app grid post-login.
    // Other items are placeholders for future pages; they simply highlight
    // in the sidebar without switching content until those pages exist.

    if (id == "workspace") {
        // Only switch if we are already logged in
        if (SessionManager::instance().isLoggedIn())
            switchToPage(2);
    }
    // "security", "tools", "store" — reserved for future stacked pages.
    // Update the sidebar highlight regardless so the UI feels responsive.
    m_nav->setActiveItem(id);
}
