// ui/MainWindow.h
#pragma once
#include <QMainWindow>
#include <QSystemTrayIcon>

#include <QStackedWidget>
#include "ui/navigation/NavigationWidget.h"
#include "ui/pages/ServerConfigWidget.h"
#include "ui/pages/LoginWidget.h"
#include "ui/pages/AppGridWidget.h"
#include <models/UserSession.h>
#include "models/VpnConf.h"


class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onConfigSaved(const ServerConfig& cfg);
    void onTestConnection();
    void onLoginSuccess(const QString& token);
    //void onLoginResult(bool ok, const UserSession& session, const QString& err);
    void onVpnConfFetched(bool success, const VpnConfig& config, const QString& errorMsg);
    void onUnauthorized();
    void onLogout();
    void onAppQuit();
    void onNavItemSelected(const QString& id);

    void addTestButtons();
private:
    void setupUi();
  void setupTray();
    void closeEvent(QCloseEvent* event);
    void setupConnections();
    void determineInitialState();
    void switchToPage(int index);   // 0=config, 1=login, 2=appgrid

    NavigationWidget*  m_nav;
    QStackedWidget*    m_stack;
    ServerConfigWidget* m_serverCfg;
    LoginWidget*       m_login;
    AppGridWidget*     m_appGrid;

	// In private members:
	QSystemTrayIcon* m_trayIcon = nullptr;
	QMenu* m_trayMenu = nullptr;

	// In private slots:
	void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
	void toggleWindowVisibility();
};
