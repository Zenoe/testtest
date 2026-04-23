// ui/MainWindow.h
#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include "ui/navigation/NavigationWidget.h"
#include "ui/pages/ServerConfigWidget.h"
#include "ui/pages/LoginWidget.h"
#include "ui/pages/AppGridWidget.h"
#include <models/UserSession.h>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onConfigSaved(const ServerConfig& cfg);
    void onTestConnection();
    void onLoginSuccess(const QString& token);
    //void onLoginResult(bool ok, const UserSession& session, const QString& err);
    void onLogout();
    void onAppQuit();
    void onNavItemSelected(const QString& id);

private:
    void setupUi();
    void setupConnections();
    void determineInitialState();
    void switchToPage(int index);   // 0=config, 1=login, 2=appgrid

    NavigationWidget*  m_nav;
    QStackedWidget*    m_stack;
    ServerConfigWidget* m_serverCfg;
    LoginWidget*       m_login;
    AppGridWidget*     m_appGrid;
};
