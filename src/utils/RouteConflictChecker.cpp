#include "RouteConflictChecker.h"

// Must come before any other Windows includes
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <ws2ipdef.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#include <QStringList>

#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>

#include "utils/logger.h"

// ─── Internal helpers ────────────────────────────────────────────────────────

namespace {

    struct ParsedCidr {
        // Stored in host-byte order for arithmetic.
        bool     isV6 = false;
        quint32  addr4 = 0;          // IPv4
        quint8   prefix = 0;
        // IPv6 — 128-bit stored as two 64-bit halves (host order)
        std::array<quint8, 16> addr6{};
    };

    /// Returns a host-order mask for a given IPv4 prefix length.
    static quint32 maskFromPrefix4(int prefix)
    {
        if (prefix == 0)  return 0u;
        if (prefix >= 32) return 0xFFFFFFFFu;
        return ~((1u << (32 - prefix)) - 1u);
    }

    /// Parse "a.b.c.d/prefix" or "addr6/prefix".  Returns nullopt on failure.
    static std::optional<ParsedCidr> parseCidr(const QString& cidr)
    {
        const int slash = cidr.lastIndexOf(u'/');
        if (slash < 0) {
            spdlog::warn("[RouteCheck] CIDR missing prefix length, skipping: '{}'",
                cidr.toStdString());
            return std::nullopt;
        }

        bool ok = false;
        const int prefixLen = cidr.mid(slash + 1).toInt(&ok);
        if (!ok || prefixLen < 0 || prefixLen > 128) {
            spdlog::warn("[RouteCheck] Invalid prefix length in '{}', skipping",
                cidr.toStdString());
            return std::nullopt;
        }

        const QString addrPart = cidr.left(slash);
        QHostAddress host(addrPart);
        if (host.isNull()) {
            spdlog::warn("[RouteCheck] Cannot parse address part of '{}', skipping",
                cidr.toStdString());
            return std::nullopt;
        }

        ParsedCidr result;
        result.prefix = static_cast<quint8>(prefixLen);

        if (host.protocol() == QAbstractSocket::IPv4Protocol) {
            result.isV6 = false;
            result.addr4 = host.toIPv4Address(); // already host-order via Qt
        }
        else {
            result.isV6 = true;
            const Q_IPV6ADDR raw = host.toIPv6Address();
            std::memcpy(result.addr6.data(), &raw, 16);
        }

        return result;
    }

    /// Returns true when the two IPv4 CIDRs share at least one address.
    static bool overlapsV4(const ParsedCidr& a, const ParsedCidr& b)
    {
        Q_ASSERT(!a.isV6 && !b.isV6);
        // Use the more-specific prefix to test containment in both directions.
        const int   shorterPrefix = (std::min)(a.prefix, b.prefix);
        const quint32 mask = maskFromPrefix4(shorterPrefix);
        return (a.addr4 & mask) == (b.addr4 & mask);
    }

    /// Returns true when the two IPv6 CIDRs share at least one address.
    static bool overlapsV6(const ParsedCidr& a, const ParsedCidr& b)
    {
        Q_ASSERT(a.isV6 && b.isV6);
        const int shorterPrefix = std::min<int>(a.prefix, b.prefix);

        int fullBytes = shorterPrefix / 8;
        int remainBits = shorterPrefix % 8;

        // Compare full bytes
        if (fullBytes > 0 && std::memcmp(a.addr6.data(), b.addr6.data(), fullBytes) != 0)
            return false;

        // Compare the partial byte (if any)
        if (remainBits > 0) {
            const quint8 mask = static_cast<quint8>(0xFF00u >> remainBits) & 0xFF;
            if ((a.addr6[fullBytes] & mask) != (b.addr6[fullBytes] & mask))
                return false;
        }
        return true;
    }

    /// Convert a MIB_IPFORWARD_ROW2 destination to a ParsedCidr for comparison.
    static std::optional<ParsedCidr> rowToCidr(const MIB_IPFORWARD_ROW2& row)
    {
        ParsedCidr cidr;
        cidr.prefix = static_cast<quint8>(row.DestinationPrefix.PrefixLength);

        if (row.DestinationPrefix.Prefix.si_family == AF_INET) {
            cidr.isV6 = false;
            // ntohl: the struct holds network-byte order
            cidr.addr4 = ntohl(row.DestinationPrefix.Prefix.Ipv4.sin_addr.s_addr);
        }
        else if (row.DestinationPrefix.Prefix.si_family == AF_INET6) {
            cidr.isV6 = true;
            std::memcpy(cidr.addr6.data(),
                row.DestinationPrefix.Prefix.Ipv6.sin6_addr.s6_addr, 16);
        }
        else {
            return std::nullopt;
        }
        return cidr;
    }

    /// Format a MIB_IPFORWARD_ROW2 gateway as a human-readable string.
    static QString gatewayToString(const MIB_IPFORWARD_ROW2& row)
    {
        char buf[INET6_ADDRSTRLEN] = {};
        if (row.NextHop.si_family == AF_INET) {
            inet_ntop(AF_INET, &row.NextHop.Ipv4.sin_addr, buf, sizeof(buf));
        }
        else if (row.NextHop.si_family == AF_INET6) {
            inet_ntop(AF_INET6, &row.NextHop.Ipv6.sin6_addr, buf, sizeof(buf));
        }
        else {
            return QStringLiteral("(unknown)");
        }
        return QString::fromLatin1(buf);
    }

    /// Format a MIB_IPFORWARD_ROW2 destination as "addr/prefix".
    static QString destinationToString(const MIB_IPFORWARD_ROW2& row)
    {
        char buf[INET6_ADDRSTRLEN] = {};
        const auto& pfx = row.DestinationPrefix;
        if (pfx.Prefix.si_family == AF_INET)
            inet_ntop(AF_INET, &pfx.Prefix.Ipv4.sin_addr, buf, sizeof(buf));
        else if (pfx.Prefix.si_family == AF_INET6)
            inet_ntop(AF_INET6, &pfx.Prefix.Ipv6.sin6_addr, buf, sizeof(buf));
        else
            return QStringLiteral("(unknown)");

        return QStringLiteral("%1/%2").arg(QString::fromLatin1(buf)).arg(pfx.PrefixLength);
    }

} // anonymous namespace


// ─── Public API ──────────────────────────────────────────────────────────────

RouteCheckResult checkRouteConflicts(
    const QString& ifaceAddress,
    const QStringList& allowedIPs)
{
    spdlog::info("[RouteCheck] Starting route conflict check");
    spdlog::debug("[RouteCheck] iface.address  = '{}'", ifaceAddress.toStdString());
    for (const auto& ip : allowedIPs)
        spdlog::debug("[RouteCheck] allowedIP      = '{}'", ip.toStdString());

    // ── 1. Parse the WireGuard CIDRs we need to protect ──────────────────────

    struct TaggedCidr {
        ParsedCidr cidr;
        QString    original;
        QString    source;
    };

    std::vector<TaggedCidr> wgCidrs;
    wgCidrs.reserve(static_cast<std::size_t>(allowedIPs.size()) + 1u);

    auto tryParse = [&](const QString& cidr, const QString& source) {
        if (auto parsed = parseCidr(cidr))
            wgCidrs.push_back({ *parsed, cidr, source });
        };

    tryParse(ifaceAddress, QStringLiteral("iface.address"));
    for (const auto& ip : allowedIPs)
        tryParse(ip, QStringLiteral("peer.allowedIPs"));

    if (wgCidrs.empty()) {
        spdlog::error("[RouteCheck] No valid CIDRs to check — aborting route validation");
        return {};
    }

    // ── 2. Fetch the system routing table (IPv4 + IPv6) ──────────────────────

    MIB_IPFORWARD_TABLE2* rawTable = nullptr;
    const DWORD rc = GetIpForwardTable2(AF_UNSPEC, &rawTable);
    if (rc != NO_ERROR || rawTable == nullptr) {
        spdlog::error("[RouteCheck] GetIpForwardTable2 failed, error={}", rc);
        return {};
    }

    // Wrap in a unique_ptr so the OS-allocated block is freed on any exit path.
    struct TableDeleter {
        void operator()(MIB_IPFORWARD_TABLE2* p) const { FreeMibTable(p); }
    };
    std::unique_ptr<MIB_IPFORWARD_TABLE2, TableDeleter> table(rawTable);

    spdlog::debug("[RouteCheck] Route table contains {} entries", table->NumEntries);

    // ── 3. Check every route entry against every WireGuard CIDR ─────────────

    RouteCheckResult result;

    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const MIB_IPFORWARD_ROW2& row = table->Table[i];

        auto routeCidr = rowToCidr(row);
        if (!routeCidr) continue;   // skip unsupported address families

        for (const auto& wg : wgCidrs) {
            // Skip family mismatches — no overlap possible
            if (routeCidr->isV6 != wg.cidr.isV6) continue;

            bool conflict = false;
            if (routeCidr->isV6)
                conflict = overlapsV6(*routeCidr, wg.cidr);
            else
                conflict = overlapsV4(*routeCidr, wg.cidr);

            if (conflict) {
                const QString existingNet = destinationToString(row);
                const QString gateway = gatewayToString(row);

                spdlog::warn(
                    "[RouteCheck] Conflict detected: existing route {} (gw {}) "
                    "overlaps WireGuard CIDR '{}' (source: {})",
                    existingNet.toStdString(),
                    gateway.toStdString(),
                    wg.original.toStdString(),
                    wg.source.toStdString());

                result.conflicts.push_back({
                    existingNet,
                    gateway,
                    wg.original,
                    wg.source
                    });
            }
        }
    }

    if (result.hasConflicts()) {
        spdlog::error("[RouteCheck] {} conflict(s) found — config must not be applied",
            result.conflicts.size());
    }
    else {
        spdlog::info("[RouteCheck] No route conflicts found — safe to apply config");
    }

    return result;
}