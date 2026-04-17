#include "AppLauncher.h"
#include <QProcess>
#include <QFileInfo>
#include <QString>

// Windows-native headers for ShellExecuteW and elevation
#include <windows.h>
#include <shellapi.h>

// ── Singleton ─────────────────────────────────────────────────────────────────

AppLauncher& AppLauncher::instance() {
    static AppLauncher inst;
    return inst;
}

AppLauncher::AppLauncher(QObject* parent)
    : QObject(parent)
{}

// ── Public API ────────────────────────────────────────────────────────────────

bool AppLauncher::launch(const AppEntry& app) {
    if (!validateEntry(app)) return false;

    // Prefer QProcess::startDetached for standard executables.
    // It is cross-process safe and does not block the UI thread.
    bool started = QProcess::startDetached(app.executablePath, {});

    if (!started) {
        // Fall back to ShellExecuteW for cases where the executable path
        // has a file association (e.g. a .bat, .cmd, or registered handler)
        // or needs the shell to resolve environment variables in the path.
        started = shellExecute(app.executablePath, false);
    }

    if (started)
        emit launchSucceeded(app.displayName);
    else
        emit launchFailed(app.displayName,
                          QString("Failed to start: %1")
                          .arg(app.executablePath));

    return started;
}

bool AppLauncher::launchElevated(const AppEntry& app) {
    if (!validateEntry(app)) return false;

    // ShellExecuteW with the "runas" verb triggers a UAC elevation prompt.
    // QProcess cannot do this — it inherits the parent process token.
    const bool started = shellExecute(app.executablePath, true);

    if (started)
        emit launchSucceeded(app.displayName);
    else
        emit launchFailed(app.displayName,
                          QString("Elevation failed or was cancelled: %1")
                          .arg(app.executablePath));

    return started;
}

// ── Private helpers ───────────────────────────────────────────────────────────

bool AppLauncher::validateEntry(const AppEntry& app) {
    if (app.executablePath.trimmed().isEmpty()) {
        emit launchFailed(app.displayName, "Executable path is empty.");
        return false;
    }

    // Only validate existence for absolute paths.
    // Relative paths and UNC paths are left to the shell to resolve.
    const QFileInfo fi(app.executablePath);
    if (fi.isAbsolute() && !fi.exists()) {
        emit launchFailed(app.displayName,
                          QString("Executable not found: %1")
                          .arg(app.executablePath));
        return false;
    }

    return true;
}

bool AppLauncher::shellExecute(const QString& path, bool elevated) {
    // Convert to wide string for WinAPI
    const std::wstring wPath = path.toStdWString();
    const wchar_t* verb = elevated ? L"runas" : L"open";

    HINSTANCE result = ShellExecuteW(
        nullptr,            // parent HWND — nullptr means no modal UAC dialog owner
        verb,               // "runas" = elevate, "open" = normal
        wPath.c_str(),      // executable path
        nullptr,            // parameters — none; server-loaded apps handle their own args
        nullptr,            // working directory — inherit from shell
        SW_SHOWNORMAL       // show the window normally
    );

    // ShellExecuteW returns a value > 32 on success (legacy HINSTANCE convention)
    return (reinterpret_cast<INT_PTR>(result) > 32);
}
