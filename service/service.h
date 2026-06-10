// Service.h
#pragma once

#include <QString>
#include <QObject>
#include "ServiceResult.h"
namespace Tunnel {

    // ---------------------------------------------------------------------------
    // Service  –  wraps tunnel.dll!WireGuardTunnelService and the SCM helpers
    // needed to install / remove / start / stop the per-tunnel Windows service.
    //
    // The GUI process calls the actual service entry point in the /service branch
    // of main (see App/main.cpp).
    //
    //   Service::run()    → WireGuardTunnelService()
    //   Service::add()    → CreateService()
    //   Service::remove() → DeleteService()
    // ---------------------------------------------------------------------------
    class Service : public QObject
    {
        Q_OBJECT

    public:
        // Calls tunnel.dll!WireGuardTunnelService from the /service branch of main().
        // Blocks until the tunnel exits. Throws std::runtime_error on failure.
        static void run(const QString& configFilePath);

        // SCM helpers (called from the GUI / manager process).
        //   serviceName = "WireGuardTunnel$<TunnelName>"
        //   displayName = human-readable label shown in services.msc
        //   exePath     = absolute path to this executable
        //   configFile  = absolute path to the .conf file
    static ServiceResult add(const QString& serviceName,
                             const QString& displayName,
                             const QString& exePath,
                             const QString& configFile);
    static ServiceResult start(const QString& serviceName);
    static ServiceResult stop(const QString& serviceName);
    static ServiceResult remove(const QString& serviceName);

    signals:
        void tunnelStateChanged(bool up); // reserved for GUI integration

    private:
        // RAII wrappers – automatically close handles on scope exit.
        struct ScmHandle {
            SC_HANDLE h = nullptr;
            explicit ScmHandle(SC_HANDLE h) : h(h) {}
            ~ScmHandle() { if (h) CloseServiceHandle(h); }
            operator SC_HANDLE() const { return h; }
            explicit operator bool() const { return h != nullptr; }
            ScmHandle(const ScmHandle&) = delete;
            ScmHandle& operator=(const ScmHandle&) = delete;

            // In Service.h, inside struct ScopedScHandle:

            /*
			ScmHandle h{ OpenSCManagerW(nullptr, nullptr, access) };
            return h;---> 尝试引用已删除的函数
            Why this happens: MSVC's RVO (Return Value Optimization) is not guaranteed for all return paths when the type has a deleted copy constructor but no explicit move constructor.
            The Standard requires the compiler to consider move construction before copy on a return statement — but only if a move constructor exists.
            Without it, MSVC falls back to copy, sees it's deleted, and errors.
			Adding the move constructor lets NRVO/RVO kick in (zero-cost at runtime), or at worst the value is moved out cleanly.
            */
            ScmHandle(ScmHandle&& other) noexcept : h(other.h) { other.h = nullptr; }
            ScmHandle& operator=(ScmHandle&& other) noexcept
            {
                if (this != &other) {
                    if (h) CloseServiceHandle(h);
                    h = other.h;
                    other.h = nullptr;
                }
                return *this;
            }
        };

        static ScmHandle openSCM(DWORD access);
        static ScmHandle openService(SC_HANDLE hSCM, const QString& name, DWORD access);
    };

} // namespace Tunnel
