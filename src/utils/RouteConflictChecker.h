#pragma once

#include <QString>
#include <QStringList>
#include <QHostAddress>

#include <optional>
#include <string>
#include <vector>

// Forward-declared to avoid pulling Windows headers into every TU.
struct RouteConflict {
    QString existingNetwork;   // e.g. "10.0.0.0/8"
    QString existingGateway;   // e.g. "192.168.1.1"
    QString conflictingCidr;   // the WireGuard CIDR that overlaps
    QString source;            // "iface.address" | "peer.allowedIPs"
};

struct RouteCheckResult {
    bool hasConflicts() const { return !conflicts.empty(); }
    std::vector<RouteConflict> conflicts;
};

/// Checks the Windows routing table for entries that overlap with
/// the WireGuard interface address and peer allowed-IPs.
///
/// @param ifaceAddress   e.g. "10.13.0.1/24"
/// @param allowedIPs     list of CIDRs, e.g. {"0.0.0.0/0", "fd00::/8"}
/// @returns              RouteCheckResult — inspect .hasConflicts() before applying config
[[nodiscard]] RouteCheckResult checkRouteConflicts(
    const QString& ifaceAddress,
    const QStringList& allowedIPs,
	bool skipDefaultRouteCheck = true
    );