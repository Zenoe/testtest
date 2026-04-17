// app/Application.cpp
#include "Application.h"
#include "ui/MainWindow.h"
//#include "backend/ConfigManager.h"
#include "utils/ConfigManager.h"
#include "backend/NetworkManager.h"
#include "backend/SessionManager.h"
#include "backend/AppLauncher.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    setOrganizationName("XY");
    setApplicationName("Revelation");
    setApplicationVersion("1.0");

    // IniFormat keeps config in %APPDATA% as a plain file,
    // not in the Windows registry — easier to inspect and reset.
    QSettings::setDefaultFormat(QSettings::IniFormat);

    configureHighDpi();
    loadStylesheet();
}

Application::~Application() {
    // QObject-parented singletons clean themselves up.
    // Anything needing explicit teardown order goes here.
}

void Application::initialise() {
    // Touch every singleton so they construct in a controlled order
    // before MainWindow starts querying them.
    ConfigManager::instance();
    NetworkManager::instance();
    SessionManager::instance();
    AppLauncher::instance();

    // If a server config already exists, pre-load it into NetworkManager
    // so it is ready before MainWindow::determineInitialState() runs.
    auto& cfg = ConfigManager::instance();
    if (cfg.contains("server")) {

        // 2. If "server" is just a string:
        auto serverAddr = cfg.getSection("server");
        NetworkManager::instance().setServer(ServerConfig::fromJson(serverAddr));

        // 3. OR, if "server" is a complex object:
        nlohmann::json serverSection = cfg.getSection("server");
        std::string host = serverSection.value("host", "localhost");
        int port = serverSection.value("port", 8080);
    }
    //if (cfg.get("server", "") != "")
    //    NetworkManager::instance().setServer(cfg.get<std::string>("server", ""));
}

void Application::configureHighDpi() {
    // PassThrough lets Qt use the exact OS scale factor (e.g. 1.5×)
    // rather than rounding to the nearest integer, giving crisp rendering
    // on 150 % displays common on Windows laptops.
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
}

void Application::loadStylesheet() {
    QFile qss(":/style.qss");
    if (qss.open(QIODevice::ReadOnly))
        setStyleSheet(qss.readAll());
}
