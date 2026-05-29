// MainWindow.test.cpp
#include "MainWindow.h"

#include "utils/ConfigManager.h"
#include "backend/NetworkManager.h"
#include "backend/SessionManager.h"
#include "backend/VpnManager.h"
#include <QMenu>
#include <QAction>
#include <QWidget>
#include <QFileInfo>
#include <QToolBar>
#include <QMessageBox>
#include "utils/logger.h"

#include "utils/RouteManager.h"
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

        const QString confPath = SessionManager::instance().vpnConfPath();
        const QString endpoint = "192.168.1.1:1194"; // Replace with actual endpoint if needed
        VpnManager::instance().connectVpn(endpoint, confPath);  
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

        const QString confPath = SessionManager::instance().vpnConfPath();
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

    auto* btnAppGrid = new QPushButton("deleteroute", this);
    btnAppGrid->setFixedHeight(30);
    QObject::connect(btnAppGrid, &QPushButton::clicked, [this]() {
        const QString vpnConfPath = SessionManager::instance().vpnConfPath();
        const QString ifalias = QFileInfo(vpnConfPath).baseName();
        const std::optional<NET_LUID> ifaceLuid = luidFromAdapterAlias(ifalias);

        auto result = SessionManager::instance().getVpnConf("192.168.1.1:1194");

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
        else {
            // this branch is for testing
            const std::optional<NET_LUID> ifaceLuid2 = luidFromAdapterAlias("XYJVvXbznIrjdZFDMG");

            QString peerIP = "10.8.0.15";
            QStringList allowedIPs = QString("10.9.0.0/16,17.1.1.0/24,33.0.0.2/32,37.3.3.0/24,192.168.1.101/32").split(",");
			if (!deleteWireGuardRoutes(peerIP, allowedIPs, ifaceLuid2.value())) {
				// warn user, flag for manual cleanup, etc.
			}
            // Apply config
            const AddRouteResult result = addDefaultRoute(peerIP, ifaceLuid2.value());
            if (!result.success) {
                spdlog::error("[VPN] Failed to add default route: {}",
                    result.errorMessage.toStdString());
                // surface to UI / abort tunnel bring-up
                return;
            }
            // Store installedRow in VpnSession so teardown can remove it precisely:
            //   DeleteIpForwardEntry2(&session.defaultRouteRow);
			SessionManager::instance().setInstalledRow(result.installedRow);
        }

        //deleteRouteEntry({ "192.168.1.101/32", "", ifaceLuid.value()});
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
