// route_manager.cpp
// 路由表 0.0.0.0       0.0.0.0       192.168.1.1  192.168.1.100  10
//Windows 内部还会跟着一个：/NET_LUID : 64位唯一ID, InterfaceIndex：32 位临时接口号，重启会变
// nic ip might change, but LUID is stable and unique for the adapter until it's removed from the system.
// So we can use LUID to identify the adapter for route cleanup even if the IP changed or was removed.
// For installation, we should use the current IP to find the LUID and then use that LUID for all route entries, so they stay correctly associated with the adapter even if its IP changes later.
// Single entry — throws on real failure, silent on NOT_FOUND
/*
try {
    deleteRouteEntry({ "10.13.0.0/24", "10.13.0.1", ifaceLuid });
}
catch (const std::system_error& ex) {
    spdlog::error("Route deletion failed: {} ({})", ex.what(), ex.code().value());
}

// Full WireGuard cleanup — best-effort, returns false if anything failed
if (!deleteWireGuardRoutes(config.iface.address, config.peer.allowedIPs, ifaceLuid)) {
    // warn user, flag for manual cleanup, etc.
}
*/

#include "RouteManager.h"

#include <QHostAddress>

#include <array>
#include <cstring>
#include <optional>
#include <system_error>
#include <vector>

#include "utils/logger.h"

// ─── Internal helpers ────────────────────────────────────────────────────────

std::optional<NET_LUID> luidFromAdapterAlias(const QString& alias)
{
    spdlog::debug("[LuidResolver] adapter alias: {}", alias.toStdString());
    const std::wstring wide = alias.toStdWString();
    NET_LUID luid{};
    const NETIO_STATUS rc =
        ConvertInterfaceAliasToLuid(wide.c_str(), &luid);
    if (rc != NO_ERROR) {
        spdlog::error("[LuidResolver] ConvertInterfaceAliasToLuid('{}') failed, error={}",
            alias.toStdString(), rc);
        return std::nullopt;
    }
    spdlog::debug("[LuidResolver] '{}' → LUID {:#018x}",
        alias.toStdString(), luid.Value);
    return luid;
}
    NET_LUID luidFromAdapterName(const QString& friendlyName)
    {
        spdlog::debug("adapter name: {}", friendlyName.toStdString());
        const std::wstring wide = friendlyName.toStdWString();

        NET_LUID luid{};
        const NETIO_STATUS rc =
            ConvertInterfaceNameToLuidW(wide.c_str(), &luid);

        if (rc != NO_ERROR) {
            spdlog::error("[LuidResolver] ConvertInterfaceNameToLuidW('{}') failed, error={}",
                friendlyName.toStdString(), rc);
            //throw std::system_error(static_cast<int>(rc), std::system_category(),
            //    "Cannot resolve LUID for adapter '" +
            //    friendlyName.toStdString() + "'");
        }

        spdlog::debug("[LuidResolver] '{}' → LUID {:#018x}",
            friendlyName.toStdString(), luid.Value);
        return luid;
    }
namespace {


    // NET_LUID is a union { ULONG64 Value; struct { ... } Info; }.
    // PowerShell's 'NetLuid' column is that same raw 64-bit integer, so this is
	// a straight assignment — no OS call needed.
    /*
    NET_LUID luidFromRawValue(ULONG64 rawValue) noexcept
    {
        NET_LUID luid{};
        luid.Value = rawValue;
        spdlog::debug("[LuidResolver] raw LUID {:#018x} used directly", luid.Value);
        return luid;
    }*/

    /// Everything the Windows IP Helper API needs to identify a route row.
    struct ResolvedRoute {
        SOCKADDR_INET destination{};
        UINT8         prefixLength = 0;
        SOCKADDR_INET nextHop{};
        NET_LUID      luid{};
    };

    /// Parse "addr/prefix" into address + prefix length.
    /// Returns nullopt and logs a warning on any parse failure.
    static std::optional<std::pair<QHostAddress, int>> parseCidrStrict(const QString& cidr){
        const int slash = cidr.lastIndexOf(u'/');
        if (slash < 0) {
            spdlog::warn("[RouteManager] CIDR '{}' has no prefix length",
                cidr.toStdString());
            //return std::nullopt;
			// treat as host route with max prefix length (32 for IPv4, 128 for IPv6)
            QHostAddress addr(cidr);
			return std::make_pair(addr, addr.protocol() == QAbstractSocket::IPv4Protocol ? 32 : 128);
        }

        bool ok = false;
        const int prefix = cidr.mid(slash + 1).toInt(&ok);
        if (!ok || prefix < 0 || prefix > 128) {
            spdlog::warn("[RouteManager] CIDR '{}' has invalid prefix length",
                cidr.toStdString());
            return std::nullopt;
        }

        QHostAddress addr(cidr.left(slash));
        if (addr.isNull()) {
            spdlog::warn("[RouteManager] CIDR '{}' has unparseable address part",
                cidr.toStdString());
            return std::nullopt;
        }

        return std::make_pair(addr, prefix);
    }

    /// Fill a SOCKADDR_INET from a QHostAddress.
    /// Returns false if the address family is not IPv4 or IPv6.
    static bool fillSockaddr(SOCKADDR_INET& out, const QHostAddress& addr)
    {
        std::memset(&out, 0, sizeof(out));

        if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
            out.si_family = AF_INET;
            out.Ipv4.sin_family = AF_INET;
            out.Ipv4.sin_addr.s_addr = htonl(addr.toIPv4Address());
            return true;
        }

        if (addr.protocol() == QAbstractSocket::IPv6Protocol) {
            out.si_family = AF_INET6;
            out.Ipv6.sin6_family = AF_INET6;
            const Q_IPV6ADDR raw = addr.toIPv6Address();
            std::memcpy(&out.Ipv6.sin6_addr, &raw, 16);
            return true;
        }

        return false;
    }

    /// Build a ResolvedRoute from human-readable strings.
    static std::optional<ResolvedRoute> resolveRoute(const QString& destinationCidr,
            const QString& nextHop,
            NET_LUID       luid)
    {
        auto parsed = parseCidrStrict(destinationCidr);
        if (!parsed) return std::nullopt;

        ResolvedRoute r;
        r.prefixLength = static_cast<UINT8>(parsed->second);
        r.luid = luid;

        if (!fillSockaddr(r.destination, parsed->first)) {
            spdlog::warn("[RouteManager] Unsupported address family in '{}'",
                destinationCidr.toStdString());
            return std::nullopt;
        }

        if (!nextHop.isEmpty()) {
            QHostAddress hopAddr(nextHop);
            if (hopAddr.isNull()) {
                spdlog::warn("[RouteManager] Cannot parse next-hop '{}'",
                    nextHop.toStdString());
                return std::nullopt;
            }
            if (!fillSockaddr(r.nextHop, hopAddr)) {
                spdlog::warn("[RouteManager] Unsupported address family in next-hop '{}'",
                    nextHop.toStdString());
                return std::nullopt;
            }
        }
        else {
            // On-link: family must match destination so the OS accepts the row.
            r.nextHop.si_family = r.destination.si_family;
        }

        return r;
    }

    // Small helper to produce a log-friendly string from a ResolvedRoute address.
    static std::string destinationCidr_from(const ResolvedRoute& r)
    {
        char buf[INET6_ADDRSTRLEN] = {};
        if (r.destination.si_family == AF_INET)
            inet_ntop(AF_INET, &r.destination.Ipv4.sin_addr, buf, sizeof(buf));
        else
            inet_ntop(AF_INET6, &r.destination.Ipv6.sin6_addr, buf, sizeof(buf));
        return std::string(buf) + "/" + std::to_string(r.prefixLength);
    }
    /// Locate a matching row in the routing table and delete it.
    /// Returns ERROR_NOT_FOUND if no matching row exists (not an error for cleanup).
    static DWORD deleteResolvedRoute(const ResolvedRoute& r)
    {
        // GetBestRoute2 finds the single best matching row; for exact deletion
        // we need to scan the full table to honour the caller's LUID constraint.
        MIB_IPFORWARD_TABLE2* rawTable = nullptr;
        DWORD rc = GetIpForwardTable2(r.destination.si_family, &rawTable);
        if (rc != NO_ERROR) {
            spdlog::error("[RouteManager] GetIpForwardTable2 failed, error={}", rc);
            return rc;
        }

        struct TableDeleter {
            void operator()(MIB_IPFORWARD_TABLE2* p) const { FreeMibTable(p); }
        };
        std::unique_ptr<MIB_IPFORWARD_TABLE2, TableDeleter> table(rawTable);

        const bool checkLuid = (r.luid.Value != 0);

        for (ULONG i = 0; i < table->NumEntries; ++i) {
            const MIB_IPFORWARD_ROW2& row = table->Table[i];

            // 1. Destination address family
            if (row.DestinationPrefix.Prefix.si_family != r.destination.si_family)
                continue;

            // 2. Prefix length
            if (row.DestinationPrefix.PrefixLength != r.prefixLength)
                continue;

            // 3. Destination address bytes
            if (r.destination.si_family == AF_INET) {
                if (row.DestinationPrefix.Prefix.Ipv4.sin_addr.s_addr
                    != r.destination.Ipv4.sin_addr.s_addr)
                    continue;
            }
            else {
                if (std::memcmp(&row.DestinationPrefix.Prefix.Ipv6.sin6_addr,
                    &r.destination.Ipv6.sin6_addr, 16) != 0)
                    continue;
            }

            // 4. Next-hop (skip if caller supplied an on-link / zeroed hop)
            const bool hopIsZero = [&] {
                if (r.nextHop.si_family == AF_INET)
                    return r.nextHop.Ipv4.sin_addr.s_addr == 0;
                std::array<quint8, 16> zeros{};
                return std::memcmp(&r.nextHop.Ipv6.sin6_addr, zeros.data(), 16) == 0;
                }();

            if (!hopIsZero) {
                if (row.NextHop.si_family != r.nextHop.si_family)
                    continue;

                if (r.nextHop.si_family == AF_INET) {
                    if (row.NextHop.Ipv4.sin_addr.s_addr
                        != r.nextHop.Ipv4.sin_addr.s_addr)
                        continue;
                }
                else {
                    if (std::memcmp(&row.NextHop.Ipv6.sin6_addr,
                        &r.nextHop.Ipv6.sin6_addr, 16) != 0)
                        continue;
                }
            }

            // 5. Interface LUID (optional)
            if (checkLuid && row.InterfaceLuid.Value != r.luid.Value)
                continue;

            // ── Match found — delete it ──────────────────────────────────────────
            MIB_IPFORWARD_ROW2 toDelete = row;   // copy: DeleteIpForwardEntry2
            // takes a non-const pointer
            rc = DeleteIpForwardEntry2(&toDelete);
            if (rc == NO_ERROR) {
                spdlog::info("[RouteManager] Deleted route {}", destinationCidr_from(r));
            }
            else {
                spdlog::error("[RouteManager] DeleteIpForwardEntry2 failed, error={}", rc);
            }
            return rc;
        }

        return ERROR_NOT_FOUND;
    }


} // anonymous namespace


// ─── Public API ──────────────────────────────────────────────────────────────

void deleteRouteEntry(const RouteEntry& entry)
{
    spdlog::debug("[RouteManager] deleteRouteEntry: dst='{}' hop='{}' luid={}",
        entry.destinationCidr.toStdString(),
        entry.nextHop.isEmpty() ? "(on-link)" : entry.nextHop.toStdString(),
        entry.interfaceLuid.Value);

    auto resolved = resolveRoute(entry.destinationCidr,
        entry.nextHop,
        entry.interfaceLuid);
    if (!resolved)
        throw std::system_error(ERROR_INVALID_PARAMETER,
            std::system_category(),
            "Failed to resolve route entry parameters");

    const DWORD rc = deleteResolvedRoute(*resolved);

    if (rc == NO_ERROR)      return;   // success
    if (rc == ERROR_NOT_FOUND) {
        spdlog::info("[RouteManager] Route '{}' not present — nothing to delete",
            entry.destinationCidr.toStdString());
        return;                        // idempotent — not an error
    }

    throw std::system_error(static_cast<int>(rc),
        std::system_category(),
        "DeleteIpForwardEntry2 failed for " +
        entry.destinationCidr.toStdString());
}

// ─────────────────────────────────────────────────────────────────────────────

bool deleteWireGuardRoutes(const QString& ifaceAddress,
    const QStringList& allowedIPs,
    NET_LUID          ifaceLuid)
{
    spdlog::info("[RouteManager] deleteWireGuardRoutes: iface='{}', {} allowedIP(s)",
        ifaceAddress.toStdString(), allowedIPs.size());

    // ── Build the full set of CIDRs to remove ────────────────────────────────
    //
    // WireGuard installs:
    //   (a) a /32 (or /128) host route for the interface address itself
    //   (b) a network route for the interface address CIDR
    //   (c) one route per allowedIP
    //
    // We attempt all three forms for (a)/(b) — if the OS says NOT_FOUND we
    // treat that as success (idempotent cleanup).

    struct PendingDeletion {
        QString cidr;
        QString description;
    };

    std::vector<PendingDeletion> targets;

    // Parse the interface address so we can derive (a) and (b).
    auto ifaceParsed = parseCidrStrict(ifaceAddress);
    if (ifaceParsed) {
        const auto& [addr, prefix] = *ifaceParsed;

        // (a) Host route: /32 for IPv4, /128 for IPv6
        const int hostPrefix = (addr.protocol() == QAbstractSocket::IPv6Protocol)
            ? 128 : 32;
        targets.push_back({
            QStringLiteral("%1/%2").arg(addr.toString()).arg(hostPrefix),
            QStringLiteral("iface host route")
            });

        // (b) Network route: apply mask to get the network address
        if (prefix < hostPrefix) {
            QHostAddress network = addr;
            if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
                const quint32 mask = (prefix == 0) ? 0u
                    : ~((1u << (32 - prefix)) - 1u);
                const quint32 netAddr = addr.toIPv4Address() & mask;
                network = QHostAddress(netAddr);
            }
            // (IPv6 masking: rely on OS — the network route will simply be
            // NOT_FOUND if the host route was the only entry installed.)
            targets.push_back({
                QStringLiteral("%1/%2").arg(network.toString()).arg(prefix),
                QStringLiteral("iface network route")
                });
        }
    }
    else {
        spdlog::warn("[RouteManager] Could not parse iface.address '{}' — "
            "host/network routes will not be removed",
            ifaceAddress.toStdString());
    }

    // (c) AllowedIPs
    for (const auto& ip : allowedIPs)
        targets.push_back({ ip, QStringLiteral("allowedIP") });

    // ── Attempt every deletion, accumulate failures ───────────────────────────

    bool allOk = true;

    for (const auto& [cidr, desc] : targets) {
        spdlog::debug("[RouteManager] Removing {} '{}'", desc.toStdString(),
            cidr.toStdString());
        try {
            deleteRouteEntry({ cidr, /*nextHop=*/{}, ifaceLuid });
        }
        catch (const std::system_error& ex) {
            // ERROR_NOT_FOUND is handled inside deleteRouteEntry (no throw).
            // Anything reaching here is a genuine failure.
            spdlog::error("[RouteManager] Failed to remove {} '{}': {} ({})",
                desc.toStdString(), cidr.toStdString(),
                ex.what(), ex.code().value());
            allOk = false;
            // Continue — attempt remaining deletions regardless.
        }
    }

    if (allOk)
        spdlog::info("[RouteManager] All WireGuard routes removed successfully");
    else
        spdlog::error("[RouteManager] One or more WireGuard routes could not be removed");

    return allOk;
}

AddRouteResult addDefaultRoute(const QString& ifaceAddress,
    NET_LUID       ifaceLuid,
    ULONG          metric)
{
    spdlog::info("[RouteManager] addDefaultRoute: iface='{}' luid={} metric={}",
        ifaceAddress.toStdString(), ifaceLuid.Value, metric);

    // ── 1. Parse the interface address to extract the gateway ────────────────
    // "10.13.0.1/24"  → gateway 10.13.0.1, IPv4 default → 0.0.0.0/0
    // "fd00::1/64"    → gateway fd00::1,   IPv6 default → ::/0

    auto parsed = parseCidrStrict(ifaceAddress);
    if (!parsed) {
        const auto msg = QStringLiteral("Cannot parse iface address '%1'")
            .arg(ifaceAddress);
        spdlog::error("[RouteManager] addDefaultRoute: {}", msg.toStdString());
        return { false, {}, msg };
    }

    const QHostAddress gateway = parsed->first;
    const bool isV6 = (gateway.protocol() == QAbstractSocket::IPv6Protocol);

    // ── 2. Verify the LUID resolves to a real interface ──────────────────────

    MIB_IF_ROW2 ifRow{};
    ifRow.InterfaceLuid = ifaceLuid;
    if (const DWORD rc = GetIfEntry2(&ifRow); rc != NO_ERROR) {
        const auto msg = QStringLiteral("Interface LUID %1 not found (error %2)")
            .arg(ifaceLuid.Value).arg(rc);
        spdlog::error("[RouteManager] addDefaultRoute: {}", msg.toStdString());
        return { false, {}, msg };
    }

    spdlog::debug("[RouteManager] Interface resolved: '{}'",
        QString::fromWCharArray(ifRow.Description).toStdString());

    // ── 3. Build the MIB_IPFORWARD_ROW2 ─────────────────────────────────────

    MIB_IPFORWARD_ROW2 row{};
    InitializeIpForwardEntry(&row);   // zero + set sensible defaults

    row.InterfaceLuid = ifaceLuid;
    row.Metric = metric;
    row.Protocol = MIB_IPPROTO_NETMGMT;  // "manually added"
    row.Origin = NlroManual;
    row.ValidLifetime = 0xFFFFFFFF;            // permanent
    row.PreferredLifetime = 0xFFFFFFFF;
    row.SitePrefixLength = 0;
    row.Loopback = FALSE;
    row.AutoconfigureAddress = FALSE;
    row.Publish = FALSE;
    row.Immortal = TRUE;

    // Destination: 0.0.0.0/0 (IPv4) or ::/0 (IPv6)
    row.DestinationPrefix.PrefixLength = 0;
    if (isV6) {
        row.DestinationPrefix.Prefix.si_family = AF_INET6;
        row.DestinationPrefix.Prefix.Ipv6.sin6_family = AF_INET6;
        // already zeroed by the InitializeIpForwardEntry + zero-init above
    }
    else {
        row.DestinationPrefix.Prefix.si_family = AF_INET;
        row.DestinationPrefix.Prefix.Ipv4.sin_family = AF_INET;
        row.DestinationPrefix.Prefix.Ipv4.sin_addr.s_addr = 0;
    }

    // Next-hop: the WireGuard interface address (the gateway)
    if (!fillSockaddr(row.NextHop, gateway)) {
        const auto msg = QStringLiteral("Unsupported address family for gateway '%1'")
            .arg(gateway.toString());
        spdlog::error("[RouteManager] addDefaultRoute: {}", msg.toStdString());
        return { false, {}, msg };
    }

    // ── 4. Sanity-check: is the metric actually higher than the current
    //       default route so we don't accidentally hijack traffic? ────────────

    {
        MIB_IPFORWARD_ROW2 bestRow{};
        SOCKADDR_INET      bestSrc{};
        SOCKADDR_INET      dest{};
        dest.si_family = isV6 ? AF_INET6 : AF_INET;

        if (GetBestRoute2(nullptr, 0, nullptr, &dest, 0, &bestRow, &bestSrc)
            == NO_ERROR) {
            spdlog::debug("[RouteManager] Current best default route metric={}",
                bestRow.Metric);
            if (metric <= bestRow.Metric) {
                spdlog::warn(
                    "[RouteManager] Requested metric {} is not higher than "
                    "existing default route metric {} — traffic may be "
                    "unexpectedly routed through the WireGuard interface",
                    metric, bestRow.Metric);
                // Not a hard error — the caller chose this metric deliberately.
            }
        }
    }

    // ── 5. Install the route ─────────────────────────────────────────────────

    const DWORD rc = CreateIpForwardEntry2(&row);

    if (rc == NO_ERROR) {
        spdlog::info(
            "[RouteManager] Default route installed: {}/0 via {} metric {}",
            isV6 ? "::" : "0.0.0.0",
            gateway.toString().toStdString(),
            metric);
        return { true, row, {} };
    }

    if (rc == ERROR_OBJECT_ALREADY_EXISTS) {
        spdlog::warn("[RouteManager] Default route already exists — skipping install");
        // Treat as success; populate installedRow via a lookup so the caller
        // can still reference the existing row for later deletion.
        MIB_IPFORWARD_ROW2 existing = row;
        if (GetIpForwardEntry2(&existing) == NO_ERROR)
            return { true, existing, {} };
        // If lookup fails, still report success but return the row we tried.
        return { true, row, {} };
    }

    const auto msg = QStringLiteral("CreateIpForwardEntry2 failed (error %1)").arg(rc);
    spdlog::error("[RouteManager] addDefaultRoute: {}", msg.toStdString());
    return { false, {}, msg };
}
