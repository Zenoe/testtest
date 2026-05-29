#pragma once

/*
MainWindow
└── VpnSessionManager(owns lifetime)
    └── RouteMonitor : QObject(moved to QThread)
            ├── signal : routeConflictDetected(QList<RouteConflict>)
			└── signal : monitoringError(QString)

Decision        Reason
NotifyRouteChange2 + event not polling     Kernel signals your event the instant the table changes — no wasted cycles, no latency tradeoff
CancelMibChangeNotify2 in stop()        Unblocks WaitForSingleObject immediately so the thread exits cleanly without a timeout
m_inConflict edge-detect flag           Prevents spamming routeConflictDetected on every unrelated route change (e.g. DHCP renewals)
Owned by VpnSessionManager              Lifetime tied to VPN session, not UI — survives window minimize/hide, testable independently
Qt::QueuedConnection explicit           Signals cross a thread boundary; making it explicit documents the intent and avoids subtle bugs if someone later moves objects around
*/
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

//required by CancelMibChangeNotify2 and NotifyRouteChange2, must be put in header file in this orderp, don't really understand it.
#include <ws2tcpip.h>

#include <iphlpapi.h>
#include "routeconflictchecker.h"

#include <QObject>
#include <QStringList>
#include <QThread>

#include <atomic>

#pragma comment(lib, "iphlpapi.lib")

/// Monitors the Windows routing table for changes that affect WireGuard routes.
///
/// eg:
///   auto* monitor = new RouteMonitor(ifaceAddress, allowedIPs);
///   auto* thread  = new QThread;
///   monitor->moveToThread(thread);
///   connect(thread,  &QThread::started,  monitor, &RouteMonitor::start);
///   connect(monitor, &RouteMonitor::routeConflictDetected, this, &VpnSessionManager::onRouteConflict);
///   thread->start();
///   // to stop:
///   monitor->stop();   // thread-safe
///   thread->quit();
///   thread->wait();
class RouteMonitor : public QObject
{
    Q_OBJECT

public:
    explicit RouteMonitor(QString ifaceAddress,
        QStringList allowedIPs,
        QObject* parent = nullptr);
    ~RouteMonitor() override;

    /// Thread-safe — may be called from any thread.
    void stop();

public slots:
    /// Call this slot from QThread::started.
    void start();

signals:
    /// Emitted when a previously-clean route table now has conflicts.
    void routeConflictDetected(QList<RouteConflict> conflicts);

    /// Emitted when a previous conflict is now resolved.
    void routeConflictResolved();

    /// Emitted if the OS notification mechanism fails unrecoverably.
    void monitoringError(const QString& reason);

private:
    void runLoop();
    void evaluate();

    QString     m_ifaceAddress;
    QStringList m_allowedIPs;

    std::atomic<bool> m_stop{ false };

    // NotifyRouteChange2 handle — kept so we can cancel it on stop()
    HANDLE m_notifyHandle{ nullptr };

    // Tracks whether we were already in a conflict state,
    // to avoid emitting redundant signals on every route event.
    bool m_inConflict{ false };
};