// Service.cpp
#include "Service.h"
#include "slogger.h"         // LOG_WIN32_ERROR, spdlog already configured here

#include <stdexcept>
#include <string>
#include <spdlog/spdlog.h>

// ── tunnel.dll export signature ─────────────────────────────────────────────
using WireGuardTunnelServiceFn = BOOL(WINAPI*)(LPCWSTR configFile);

// Resolves WireGuardTunnelService once; raises on failure.
// Called lazily from run() via a function-local static.
static WireGuardTunnelServiceFn resolveTunnelFn()
{
    spdlog::debug("[Service] Loading tunnel.dll");

    HMODULE hTunnel = GetModuleHandleW(L"tunnel.dll");
    if (!hTunnel)
        hTunnel = LoadLibraryW(L"tunnel.dll");

    if (!hTunnel) {
        LOG_WIN32_ERROR("LoadLibraryW(tunnel.dll)");
        throw std::runtime_error("Failed to load tunnel.dll");
    }

    auto fn = reinterpret_cast<WireGuardTunnelServiceFn>(
        GetProcAddress(hTunnel, "WireGuardTunnelService"));

    if (!fn) {
        LOG_WIN32_ERROR("GetProcAddress(WireGuardTunnelService)");
        throw std::runtime_error("WireGuardTunnelService not found in tunnel.dll");
    }

    spdlog::debug("[Service] WireGuardTunnelService resolved at {:p}",
        reinterpret_cast<void*>(fn));
    return fn;
}

namespace Tunnel {

    // ── Internal helpers ─────────────────────────────────────────────────────────

    Service::ScmHandle Service::openSCM(DWORD access)
    {
        ScmHandle h{ OpenSCManagerW(nullptr, nullptr, access) };

        // process null h in the caller
        //if (!h) {
        //    LOG_WIN32_ERROR("OpenSCManagerW");
        //    throw std::runtime_error("OpenSCManager failed");
        //}
        return h;
    }

    Service::ScmHandle Service::openService(SC_HANDLE hSCM,
        const QString& name,
        DWORD access)
    {
        ScmHandle h{ OpenServiceW(hSCM,
                                  reinterpret_cast<LPCWSTR>(name.utf16()),
                                  access) };
        // process null h in the caller
        //if (!h) {
        //    LOG_WIN32_ERROR("OpenServiceW");
        //    throw std::runtime_error(
        //        "OpenService failed for: " + name.toStdString());
        //}
        return h;
    }

    // ── Public API ───────────────────────────────────────────────────────────────

    // run() – entry point for the /service branch of main().
    // Calls tunnel.dll!WireGuardTunnelService and blocks until the tunnel exits.
    void Service::run(const QString& configFilePath)
    {
        static const WireGuardTunnelServiceFn fn = resolveTunnelFn();

        spdlog::info("[Service] Starting WireGuardTunnelService, config={}",
            configFilePath.toStdString());

        if (!fn(reinterpret_cast<LPCWSTR>(configFilePath.utf16()))) {
            LOG_WIN32_ERROR("WireGuardTunnelService");
            throw std::runtime_error("WireGuardTunnelService returned FALSE");
        }

        spdlog::info("[Service] WireGuardTunnelService exited cleanly");
    }

    // add() – installs the per-tunnel Windows service.
    //
    // Service parameters (per WireGuard-windows README):
    //   Name   : "WireGuardTunnel$<TunnelName>"
    //   Type   : SERVICE_WIN32_OWN_PROCESS
    //   Start  : SERVICE_AUTO_START
    //   Error  : SERVICE_ERROR_NORMAL
    //   Deps   : Nsi, TcpIp
    //   SID    : SERVICE_SID_TYPE_UNRESTRICTED  ← ESSENTIAL for WireGuard
    //   Binary : "<exePath> /service <configFile>"
ServiceResult Service::add(const QString& serviceName,
                           const QString& displayName,
                           const QString& exePath,
                           const QString& configFile)
{
    spdlog::info("[Service] add | name={}", serviceName.toStdString());

    ScmHandle hSCM = openSCM(SC_MANAGER_ALL_ACCESS);
    if (!hSCM) {
        const DWORD err = GetLastError();
        LOG_WIN32_ERROR("SC_MANAGER_ALL_ACCESS");
        return ServiceResult::fromWin32(err, "openSCM");
    }
    const QString cmdLine =
        QStringLiteral("\"%1\" /service \"%2\"").arg(exePath, configFile);
    static constexpr wchar_t kDeps[] = L"Nsi\0TcpIp\0";

    ScmHandle hSvc{ CreateServiceW(
        hSCM,
        reinterpret_cast<LPCWSTR>(serviceName.utf16()),
        reinterpret_cast<LPCWSTR>(displayName.utf16()),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        reinterpret_cast<LPCWSTR>(cmdLine.utf16()),
        nullptr, nullptr, kDeps, nullptr, nullptr)
    };

    if (!hSvc) {
        const DWORD err = GetLastError();
        LOG_WIN32_ERROR("CreateServiceW");
        // ERROR_SERVICE_EXISTS is soft — service was already installed
        return ServiceResult::fromWin32(err, "add: " + serviceName);
    }

    SERVICE_SID_INFO sidInfo{};
    sidInfo.dwServiceSidType = SERVICE_SID_TYPE_UNRESTRICTED;
    if (!ChangeServiceConfig2W(hSvc, SERVICE_CONFIG_SERVICE_SID_INFO, &sidInfo)) {
        const DWORD err = GetLastError();
        LOG_WIN32_ERROR("ChangeServiceConfig2W");
        return ServiceResult::fromWin32(err, "add/SID: " + serviceName);
    }

    spdlog::info("[Service] add | OK: {}", serviceName.toStdString());
    return ServiceResult::success();
}

ServiceResult Service::start(const QString& serviceName)
{
    spdlog::info("[Service] start | name={}", serviceName.toStdString());

    ScmHandle hSCM = openSCM(SC_MANAGER_CONNECT);
    if (!hSCM) {
        const DWORD err = GetLastError();
        LOG_WIN32_ERROR("SC_MANAGER_CONNECT");
        return ServiceResult::fromWin32(err, "openSCM");
    }
    ScmHandle hSvc = openService(hSCM, serviceName, SERVICE_START);

    if (!hSvc) {
        const DWORD err = GetLastError();
        LOG_WIN32_ERROR("openService");
        return ServiceResult::fromWin32(err, "start: " + serviceName);
    }
    if (!StartServiceW(hSvc, 0, nullptr)) {
        const DWORD err = GetLastError();
        LOG_WIN32_ERROR("StartServiceW");
        return ServiceResult::fromWin32(err, "start: " + serviceName);
    }

    spdlog::info("[Service] start | OK: {}", serviceName.toStdString());
    return ServiceResult::success();
}

ServiceResult Service::stop(const QString& serviceName)
{
    spdlog::info("[Service] stop | name={}", serviceName.toStdString());

    ScmHandle hSCM = openSCM(SC_MANAGER_CONNECT);
    if (!hSCM) {
        const DWORD err = GetLastError();
        LOG_WIN32_ERROR("SC_MANAGER_CONNECT");
        return ServiceResult::fromWin32(err, "openSCM");
    }
    ScmHandle hSvc = openService(hSCM, serviceName,
                                 SERVICE_STOP | SERVICE_QUERY_STATUS);

    if (!hSvc) {
        const DWORD err = GetLastError();
        LOG_WIN32_ERROR("openService");
        return ServiceResult::fromWin32(err, "stop: " + serviceName);
    }
    SERVICE_STATUS status{};
    if (!ControlService(hSvc, SERVICE_CONTROL_STOP, &status)) {
        const DWORD err = GetLastError();
        LOG_WIN32_ERROR("ControlService STOP");
        return ServiceResult::fromWin32(err, "stop: " + serviceName);
    }

    spdlog::info("[Service] stop | state={}", static_cast<DWORD>(status.dwCurrentState));
    return ServiceResult::success();
}

ServiceResult Service::remove(const QString& serviceName)
{
    spdlog::info("[Service] remove | name={}", serviceName.toStdString());

    ScmHandle hSCM = openSCM(SC_MANAGER_CONNECT);
    if (!hSCM) {
        const DWORD err = GetLastError();
        LOG_WIN32_ERROR("SC_MANAGER_CONNECT");
        return ServiceResult::fromWin32(err, "openSCM");
    }
    ScmHandle hSvc = openService(hSCM, serviceName, DELETE);

    if (!hSvc) {
        const DWORD err = GetLastError();
        LOG_WIN32_ERROR("openService");
        return ServiceResult::fromWin32(err, "stop: " + serviceName);
    }
    if (!DeleteService(hSvc)) {
        const DWORD err = GetLastError();
        LOG_WIN32_ERROR("DeleteService");
        return ServiceResult::fromWin32(err, "remove: " + serviceName);
    }

    spdlog::info("[Service] remove | OK: {}", serviceName.toStdString());
    return ServiceResult::success();
}

} 

