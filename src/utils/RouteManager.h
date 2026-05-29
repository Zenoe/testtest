// route_manager.h
#pragma once

#include "RouteConflictChecker.h"  // ParsedCidr lives here

#include <QString>
#include <QStringList>

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <ws2ipdef.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#include <system_error>

/// Describes one route entry for deletion; mirrors what the OS needs.
struct RouteEntry {
    QString destinationCidr;  ///< e.g. "10.13.0.0/24"
    QString nextHop;          ///< e.g. "10.13.0.1" — empty means on-link
    NET_LUID interfaceLuid;   ///< identifies the adapter; zeroed = match any
};

/// Deletes a single route entry from the Windows routing table.
///
/// Matches on (destination, prefixLength, nextHop, interfaceLuid).
/// If interfaceLuid is zeroed it is ignored and the first matching
/// destination+nextHop entry is removed.
///
/// @throws std::system_error on OS failure (e.g. ERROR_NOT_FOUND,
///         ERROR_ACCESS_DENIED — the latter requires elevated privileges).
void deleteRouteEntry(const RouteEntry& entry);

/// Deletes every route that the WireGuard config may have installed:
///   • the interface address host route  (iface.address /32 or /128)
///   • the interface address network route (iface.address network/prefix)
///   • every entry in peer.allowedIPs
///
/// Errors are logged and accumulated; the function attempts all deletions
/// even if some fail, then returns false if any single deletion failed.
///
/// @param ifaceAddress  e.g. "10.13.0.1/24"
/// @param allowedIPs    e.g. {"0.0.0.0/0", "fd00::/8"}
/// @param ifaceLuid     LUID of the WireGuard virtual adapter (pass zeroed to
///                      match any interface — fine for cleanup, not for install)
/// @returns true if all entries were deleted (or were already absent),
///          false if at least one deletion failed for a reason other than
///          ERROR_NOT_FOUND.
[[nodiscard]] bool deleteWireGuardRoutes(
    const QString& ifaceAddress,
    const QStringList& allowedIPs,
    NET_LUID          ifaceLuid = {});

NET_LUID luidFromAdapterName(const QString& friendlyName);
std::optional<NET_LUID> luidFromAdapterAlias(const QString& alias);

/// Result of addDefaultRoute — carries the installed row so the caller
/// can remove it precisely later without a table scan.
struct AddRouteResult {
    bool          success = false;
    MIB_IPFORWARD_ROW2 installedRow{};   ///< valid only when success == true
    QString       errorMessage;
};

/// Adds a default route (0.0.0.0/0 or ::/0) via the WireGuard interface
/// address, with a metric high enough to sit below every real system route.
///
/// Mirrors:  route add 0.0.0.0 mask 0.0.0.0 <gateway> metric 9999
///
/// The gateway is derived from ifaceAddress (the host address of the
/// WireGuard interface, e.g. "10.13.0.1/24" → gateway "10.13.0.1").
/// Both IPv4 and IPv6 interface addresses are supported.
///
/// @param ifaceAddress  e.g. "10.13.0.1/24" or "fd00::1/64"
/// @param ifaceLuid     LUID of the WireGuard virtual adapter
/// @param metric        Route metric — default 9999 intentionally exceeds
///                      typical system defaults (≤ 500)
/// @returns             AddRouteResult — inspect .success before use
[[nodiscard]] AddRouteResult addDefaultRoute(
    const QString& ifaceAddress,
    NET_LUID       ifaceLuid,
    ULONG          metric = 9999);
