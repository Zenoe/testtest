// ui/MainWindow.cpp
#include "MainWindow.h"
#include "utils/ConfigManager.h"
#include "backend/NetworkManager.h"
#include "backend/SessionManager.h"
#include "backend/VpnManager.h"
#include "secure/SecureStorageFactory.h"
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QApplication>
#include <QWidget>
#include <QFileInfo>
#include <QProcess>
#include <QToolBar>
#include <QMessageBox>
#include "utils/logger.h"

#include "utils/RouteManager.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    // setupTray();
    setupConnections();
    VpnManager::instance().initializeControlService();
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

    addTestButtons();
}

void MainWindow::setupConnections() {
    // ServerConfigWidget signals
    connect(m_serverCfg, &ServerConfigWidget::configSaved,
            this, &MainWindow::onConfigSaved);
    connect(m_serverCfg, &ServerConfigWidget::testRequested,
            this, &MainWindow::onTestConnection);

    // LoginWidget signals
    connect(m_login, &LoginWidget::loginSuccess, this, &MainWindow::onLoginSuccess);

    //connect(m_appGrid, &AppGridWidget::logoutRequested,
    //        this, &MainWindow::onLogout);
    connect(m_nav, &NavigationWidget::logoutRequested,
            this, &MainWindow::onLogout);

    // NetworkManager callbacks
    auto& net = NetworkManager::instance();
    connect(&net, &NetworkManager::serverError, this, [this](const QString& errMsg) {
        m_login->showMsg(errMsg, true);
        });
    connect(&net, &NetworkManager::connectivityResult,
            m_serverCfg, &ServerConfigWidget::onTestResult);

    connect(&net, &NetworkManager::vpnConfFetched, this, &MainWindow::onVpnConfFetched);
    connect(&net, &NetworkManager::unauthorized, this, &MainWindow::onUnauthorized);
    connect(&net, &NetworkManager::appListReady,
            m_appGrid, &AppGridWidget::onAppsReceived);

    auto& vpn = VpnManager::instance();
    connect(&vpn, &VpnManager::connected, this, &MainWindow::onVpnConnected);
    connect(&vpn, &VpnManager::errorOccurred,
            this, [this](const QString& ctx, const QString& detail) {
                const QString message =
                    QString("Failed at step [%1]: %2").arg(ctx, detail);
                m_login->showMsg(message, true);
                QMessageBox::critical(this, "VPN Error", message);
            });

    // Navigation
    connect(m_nav, &NavigationWidget::itemSelected,
            this, &MainWindow::onNavItemSelected);
    connect(qApp, &QApplication::aboutToQuit, this, &MainWindow::onAppQuit);
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
		switchToPage(1);
        //if (session.hasAutoLogin()) {
        //    switchToPage(2);
        //    NetworkManager::instance().fetchAppList(session.token());
        //} else {
        //    switchToPage(1);
        //}
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

void MainWindow::onLoginSuccess(const QString& token) {
	m_login->showMsg("登录成功，正在连接VPN...");
    NetworkManager::instance().fetchVpnConf();

    //const QString confPath = SessionManager::instance().vpnConfPath();
    // VpnManager::instance().connectVpn(confPath);
    // switchToPage(2);
    // NetworkManager::instance().fetchAppList(token);
}

void MainWindow::onVpnConfFetched(bool success, const VpnConfig& config, const QString& errorMsg)
{
    if (!success) {
      spdlog::error("Failed to fetch VPN config: {}", errorMsg.toStdString());
      m_login->showMsg("vpn配置获取失败", true);
      return;
    }
    // const QString confPath = QStandardPaths::writableLocation(
    //     QStandardPaths::AppDataLocation)
    //     + "/clientx.conf";

    const QString confPath = SessionManager::instance().vpnConfPath();
     if (!config.writeVpnConfig(confPath)) {
       spdlog::error("Failed to write VPN config to '{}'", confPath.toStdString());
       m_login->showMsg(
           QString("Failed to write VPN config: %1").arg(confPath), true);
       return;
     }

     // save allowered ips
     SessionManager::instance().setVpnConf(config.peer.endpoint, config.iface.address, config.peer.allowedIPs);

    spdlog::debug("fetchVpnConf: config written to '{}'", confPath.toStdString());
    VpnManager::instance().connectVpn(config.peer.endpoint, confPath);
}

void MainWindow::onVpnConnected(const QString& endpoint ){
	m_login->showMsg("VPN连接成功，正在资源配置...");
    // adjust route table entries
    adjustRoutes(endpoint);
	switchToPage(2);
	m_login->setLoading(false);
	NetworkManager::instance().fetchSandBoxConf();
}

void MainWindow::onUnauthorized(){
  m_login->showMsg("权限不足", true);
}

void MainWindow::onLogout() {
	spdlog::info("logout");
    const QString confPath = SessionManager::instance().vpnConfPath();

    connect(&VpnManager::instance(), &VpnManager::disconnected,
        this, [this](const QString&) {
            //SessionManager::instance().clearSession();
            switchToPage(1);
        }, Qt::SingleShotConnection);

    VpnManager::instance().disconnectVpn(confPath);
	NetworkManager::instance().logout();
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

void MainWindow::onAppQuit()
{
    // 检查是否已登录
    if (!SessionManager::instance().isLoggedIn()) {
        return;
    }

    auto storage = SecureStorageFactory::create();
    QString err;

    if (!SessionManager::instance().rememberMe()) {
        // 用户没有选择"记住我"，退出时清理存储的数据
        storage->remove(qApp->applicationDisplayName(), err);
    }
    // 如果用户选择了"记住我"，可以保留数据以便下次自动登录

    SessionManager::instance().clearSession();

    qDebug() << "Session cleared on exit";
}

void MainWindow::setupTray()
{
    // One gotcha on Windows : QSystemTrayIcon::isSystemTrayAvailable() can return false briefly at login.If you launch at startup, wrap setupTray() with a check or a short QTimer::singleShot delay(500 ms) to let the shell finish loading before creating the tray icon.

    // --- Icon: use your app resource, or fall back to a built-in Qt icon ---
    const QIcon appIcon = QIcon(":/tray.ico");   // adjust path
    // const QIcon appIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);

    m_trayIcon = new QSystemTrayIcon(appIcon, this);
    m_trayIcon->setToolTip("Revelation");

    // Context menu
    m_trayMenu = new QMenu(this);

    auto* actionShow = m_trayMenu->addAction("Show / Hide");
    m_trayMenu->addSeparator();
    auto* actionQuit = m_trayMenu->addAction("Quit");

    connect(actionShow, &QAction::triggered, this, &MainWindow::toggleWindowVisibility);
    connect(actionQuit, &QAction::triggered, qApp, &QApplication::quit);

    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->show();

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this,       &MainWindow::onTrayIconActivated);
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    // Single-click or double-click toggles the window
    if (reason == QSystemTrayIcon::Trigger ||
        reason == QSystemTrayIcon::DoubleClick)
    {
        toggleWindowVisibility();
    }
}

void MainWindow::toggleWindowVisibility()
{
    if (isVisible() && !isMinimized()) {
        hide();
    } else {
        showNormal();
        raise();
        activateWindow();
    }
}

// Override closeEvent so the X button minimizes to tray instead of quitting
void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        m_trayIcon->showMessage(
            "Revelation",
            "Right-click the icon to quit.",
            QSystemTrayIcon::Information,
            2500   // ms
        );
        event->ignore();   // don't actually close
    } else {
        event->accept();   // no tray? close normally
    }
}

void MainWindow::adjustRoutes(const QString& endpoint ) {
    const QString vpnConfPath = SessionManager::instance().vpnConfPath();
    const QString ifalias = QFileInfo(vpnConfPath).baseName();
    uint attempts = 5;
	std::optional<NET_LUID> ifaceLuid;
    for (int i = 0; i < attempts; ++i) {
		Sleep(200);  
        const std::optional<NET_LUID> ifaceLuidTemp = luidFromAdapterAlias(ifalias);
        if(ifaceLuidTemp.has_value()) {
            ifaceLuid = ifaceLuidTemp;
            spdlog::debug("Found interface LUID: {} for alias '{}'", ifaceLuid->Value, ifalias.toStdString());
            break;
		}
    }

    if(!ifaceLuid.has_value()) {
        spdlog::error("Failed to find interface for VPN config '{}'", vpnConfPath.toStdString());
        return;
	}
    auto result = SessionManager::instance().getVpnConf(endpoint);

    if (result.has_value()) {
        const auto& vpnPair = result.value();
        const QString& peerIP = vpnPair.first;
        const QStringList& allowedIPs = vpnPair.second;

        // Full WireGuard cleanup — best-effort, returns false if anything failed
        if (!deleteWireGuardRoutes(peerIP, allowedIPs, ifaceLuid.value())) {
            // warn user, flag for manual cleanup, etc.
        }
        // Use the values
        qDebug() << "Peer IP:" << peerIP;
        qDebug() << "Allowed IPs:" << allowedIPs;
    }

}
