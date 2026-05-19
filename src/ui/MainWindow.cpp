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

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    // setupTray();
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

    addTestButtons();
}
void MainWindow::addTestButtons() {
    spdlog::info("addTestButton");
    QToolBar* testToolbar = new QToolBar("Test Controls", this);
    testToolbar->setAllowedAreas(Qt::TopToolBarArea);
    addToolBar(Qt::TopToolBarArea, testToolbar);

    // Add spacing to push buttons to the right (optional)
    auto* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    testToolbar->addWidget(spacer);

    auto* btnVpn = new QPushButton("vpn start", this);
    btnVpn->setFixedHeight(30);
    QObject::connect(btnVpn, &QPushButton::clicked, this, [this]() {
        connect(&VpnManager::instance(), &VpnManager::connected,
            this, [this](const QString&) {
				spdlog::info("VPN connected successfully");
            });

        connect(&VpnManager::instance(), &VpnManager::errorOccurred,
            this, [this](const QString& ctx, const QString& detail) {
                QMessageBox::critical(this, "VPN Error",
                    QString("Failed at step [%1]: %2").arg(ctx, detail));
            });

        const QString confPath = SessionManager::instance().vpnConf();
        VpnManager::instance().connectVpn(confPath);
        });
    testToolbar->addWidget(btnVpn);

    auto* btnDisVpn = new QPushButton("vpn stop", this);
    btnDisVpn->setFixedHeight(30);
    QObject::connect(btnDisVpn, &QPushButton::clicked, this, [this]() {
        connect(&VpnManager::instance(), &VpnManager::disconnected,
            this, [this](const QString&) {
                // todo stop status thread polling
				spdlog::info("VPN disconnected");
            });

        connect(&VpnManager::instance(), &VpnManager::errorOccurred,
            this, [this](const QString& ctx, const QString& detail) {
                QMessageBox::critical(this, "VPN Error",
                    QString("Failed at step [%1]: %2").arg(ctx, detail));
            });

        const QString confPath = SessionManager::instance().vpnConf();
        VpnManager::instance().disconnectVpn(confPath);
        });
    testToolbar->addWidget(btnDisVpn);
    auto* btnServer = new QPushButton("Server Config", this);
    btnServer->setFixedHeight(30);
    QObject::connect(btnServer, &QPushButton::clicked, this, [this]() {
        //m_stack->setCurrentIndex(0);
        spdlog::info("Switched to Server Config view");
        });
    testToolbar->addWidget(btnServer);

    // Test button 2: Switch to Login view
    auto* btnLogin = new QPushButton("Login", this);
    btnLogin->setFixedHeight(30);
    QObject::connect(btnLogin, &QPushButton::clicked, [this]() {
        m_stack->setCurrentIndex(1);
        spdlog::info("Switched to Login view");
        });
    testToolbar->addWidget(btnLogin);

    // Test button 3: Switch to App Grid view
    auto* btnAppGrid = new QPushButton("App Grid", this);
    btnAppGrid->setFixedHeight(30);
    QObject::connect(btnAppGrid, &QPushButton::clicked, [this]() {
        m_stack->setCurrentIndex(2);
        spdlog::info("Switched to App Grid view");
        });
    testToolbar->addWidget(btnAppGrid);

    // Test button 4: Simulate server config change (emit signal example)
    auto* btnSimulate = new QPushButton("Simulate Config", this);
    btnSimulate->setFixedHeight(30);
    QObject::connect(btnSimulate, &QPushButton::clicked, [this]() {
        // Example: Emit a signal if widgets have appropriate signals
        spdlog::warn("Simulated server configuration change");
        // You could trigger mock data here
        });
    testToolbar->addWidget(btnSimulate);

    // Optional: Add a separator and close button to remove test toolbar
    testToolbar->addSeparator();
    auto* btnClose = new QPushButton("✕", this);
    btnClose->setFixedHeight(30);
    QObject::connect(btnClose, &QPushButton::clicked, [testToolbar]() {
        testToolbar->hide();
        spdlog::info("Test toolbar hidden");
        });
    testToolbar->addWidget(btnClose);
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
        m_login->showError(errMsg);
        });
    connect(&net, &NetworkManager::connectivityResult,
            m_serverCfg, &ServerConfigWidget::onTestResult);

    connect(&net, &NetworkManager::vpnConfFetched, this, &MainWindow::onVpnConfFetched);
    connect(&net, &NetworkManager::unauthorized, this, &MainWindow::onUnauthorized);
    connect(&net, &NetworkManager::appListReady,
            m_appGrid, &AppGridWidget::onAppsReceived);

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
    const QString confPath = SessionManager::instance().vpnConf();
    NetworkManager::instance().fetchVpnConf();
    // VpnManager::instance().connectVpn(confPath);
    // switchToPage(2);
    // NetworkManager::instance().fetchAppList(token);
}

void MainWindow::onVpnConfFetched(bool success, const VpnConfig& config, const QString& errorMsg)
{
    if (!success) {
      spdlog::error("Failed to fetch VPN config: {}", errorMsg.toStdString());
      m_login->showError("vpn配置获取失败");
      return;
    }
    // const QString confPath = QStandardPaths::writableLocation(
    //     QStandardPaths::AppDataLocation)
    //     + "/clientx.conf";

    const QString confPath = SessionManager::instance().vpnConf();
    // if (!config.writeVpnConfig(confPath)) {
    //   spdlog::error("Failed to write VPN config to '{}'", confPath.toStdString());
    //   return;
    // }

    spdlog::debug("fetchVpnConf: config written to '{}'", confPath.toStdString());
    connect(&VpnManager::instance(), &VpnManager::connected,
            this, [this](const QString&) {
                switchToPage(2);
                NetworkManager::instance().fetchSandBoxConf();
            }, Qt::SingleShotConnection);

    connect(&VpnManager::instance(), &VpnManager::errorOccurred,
            this, [this](const QString& ctx, const QString& detail) {
                QMessageBox::critical(this, "VPN Error",
                    QString("Failed at step [%1]: %2").arg(ctx, detail));
            }, Qt::SingleShotConnection);
	VpnManager::instance().connectVpn(confPath);
}

void MainWindow::onUnauthorized(){
  m_login->showError("权限不足");
}

void MainWindow::onLogout() {
	spdlog::info("logout");
    const QString confPath = SessionManager::instance().vpnConf();

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
