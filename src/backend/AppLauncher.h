#pragma once
#include <QObject>
#include "models/AppEntry.h"

class AppLauncher : public QObject {
    Q_OBJECT

public:
    static AppLauncher& instance();

    // Returns false if the process could not be started
    bool launch(const AppEntry& app);

    // Elevated launch via ShellExecuteW runas verb
    bool launchElevated(const AppEntry& app);

signals:
    void launchFailed(const QString& appName, const QString& reason);
    void launchSucceeded(const QString& appName);

private:
    explicit AppLauncher(QObject* parent = nullptr);
    ~AppLauncher() = default;

    AppLauncher(const AppLauncher&)            = delete;
    AppLauncher& operator=(const AppLauncher&) = delete;

    bool shellExecute(const QString& path, bool elevated);
    bool validateEntry(const AppEntry& app);

};
