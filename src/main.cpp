// main.cpp
#include "app/Application.h"
#include "ui/MainWindow.h"
#include "utils/ConfigManager.h"
#include <QStandardPaths>
#include <QDir>
#include "utils/logger.h"

void loadSettings()
{
	QString name = QCoreApplication::applicationName();
    // %APPDATA%\XY\XYBox\xyboxconfig.json
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(configDir);
    QString configPath = configDir + "/" + name + "config.json";

    ConfigManager::instance().init(configPath.toStdString());

#ifdef QT_DEBUG
    QString buildType = "debug";
#else
    QString buildType = "release";
#endif

    QString logsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + "/logs/" + buildType;
    QDir().mkpath(logsDir);

    QString defaultLogPath = logsDir + "/" + name + "/app.log";

    // Only write defaults if the keys don't exist yet (preserves user changes)
    ConfigManager::instance().set("window.width", 1280);
    ConfigManager::instance().set("window.height", 720);
    ConfigManager::instance().set("log.file", defaultLogPath.toStdString());
    ConfigManager::instance().set("log.max_size_mb", 10);     // 10 MB per file
    ConfigManager::instance().set("log.max_files", 5);
    ConfigManager::instance().saveNow();
    // NO shutdown() – singleton must stay alive
}


int main(int argc, char* argv[]) {
    Application app(argc, argv);
    QCoreApplication::setOrganizationName("XY");
    // QCoreApplication::setOrganizationDomain("XY.com");
    QCoreApplication::setApplicationName("Revelation");
    QCoreApplication::setApplicationVersion("1.0.0");
    loadSettings();
    setup_logging();
    spdlog::debug("main start");
    app.initialise();               // singletons + stylesheet + server pre-load

	//qDebug() << "Application started with config:" << QString::fromStdString(ConfigManager::instance().getSection("server").dump());
    MainWindow w;
    w.show();
	// spdlog::shutdown(); // shutdown before app.exec, cuase spdlog crash in slot functions
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {spdlog::shutdown();});
    return app.exec();
}
