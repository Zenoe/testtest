// routemonitor.cpp
#include "RouteMonitor.h"

//#include <ws2tcpip.h>  // required by CancelMibChangeNotify2 and NotifyRouteChange2, must be put in header file in that order, don't really understand it.

#include "utils/logger.h"

RouteMonitor::RouteMonitor(QString ifaceAddress,
    QStringList allowedIPs,
    QObject* parent)
    : QObject(parent)
    , m_ifaceAddress(std::move(ifaceAddress))
    , m_allowedIPs(std::move(allowedIPs))
{
}

RouteMonitor::~RouteMonitor()
{
    stop();
}

void RouteMonitor::stop()
{
    m_stop.store(true, std::memory_order_relaxed);

    // Unblock the WaitForSingleObject inside runLoop() immediately.
    if (m_notifyHandle) {
        CancelMibChangeNotify2(m_notifyHandle);
        m_notifyHandle = nullptr;
    }
}

// ─── Entry point (runs on the worker thread) ─────────────────────────────────

void RouteMonitor::start()
{
    spdlog::info("[RouteMonitor] Starting — watching {} WireGuard CIDR(s)",
        m_allowedIPs.size() + 1);

    // Baseline check before we even start listening.
    evaluate();

    runLoop();

    spdlog::info("[RouteMonitor] Stopped");
}

// ─── Core loop ───────────────────────────────────────────────────────────────

void RouteMonitor::runLoop()
{
    while (!m_stop.load(std::memory_order_relaxed)) {

        // Manual-reset event: NotifyRouteChange2 signals it on any route change.
        HANDLE event = CreateEventW(nullptr, /*manualReset=*/TRUE,
            /*initialState=*/FALSE, nullptr);
        if (!event) {
            const DWORD err = GetLastError();
            spdlog::error("[RouteMonitor] CreateEvent failed, error={}", err);
            emit monitoringError(
                QStringLiteral("CreateEvent failed: %1").arg(err));
            return;
        }

        // Register for the next route-table change notification.
        // AF_UNSPEC covers both IPv4 and IPv6.
        const DWORD rc = NotifyRouteChange2(
            AF_UNSPEC,
            nullptr,          // no callback — we use the event
            event,
            /*initialNotification=*/FALSE,
            &m_notifyHandle);

        if (rc != ERROR_IO_PENDING && rc != NO_ERROR) {
            spdlog::error("[RouteMonitor] NotifyRouteChange2 failed, error={}", rc);
            emit monitoringError(
                QStringLiteral("NotifyRouteChange2 failed: %1").arg(rc));
            CloseHandle(event);
            return;
        }

        spdlog::debug("[RouteMonitor] Waiting for route table change…");

        // Block until the route table changes or stop() is called.
        const DWORD waitResult = WaitForSingleObject(event, INFINITE);
        CloseHandle(event);

        if (m_stop.load(std::memory_order_relaxed))
            break;

        if (waitResult != WAIT_OBJECT_0) {
            spdlog::warn("[RouteMonitor] Unexpected wait result: {}", waitResult);
            continue;
        }

        spdlog::debug("[RouteMonitor] Route table changed — re-evaluating");
        evaluate();

        // Re-register on the next loop iteration for the following change.
    }
}

// ─── Conflict evaluation ─────────────────────────────────────────────────────

void RouteMonitor::evaluate()
{
    const RouteCheckResult result =
        checkRouteConflicts(m_ifaceAddress, m_allowedIPs);

    if (result.hasConflicts() && !m_inConflict) {
        m_inConflict = true;
        spdlog::warn("[RouteMonitor] Route conflict(s) appeared — notifying");
        emit routeConflictDetected(
            QList<RouteConflict>(result.conflicts.begin(),
                result.conflicts.end()));

    }
    else if (!result.hasConflicts() && m_inConflict) {
        m_inConflict = false;
        spdlog::info("[RouteMonitor] Route conflict(s) resolved");
        emit routeConflictResolved();
    }
}