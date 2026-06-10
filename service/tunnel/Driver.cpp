// ============================================================
//  Driver.cpp  –  WireGuard wireguard.dll C++ wrapper  (no Qt)
// ============================================================
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <ifdef.h>

#include "Driver.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include "Win32Log.h"

#pragma comment(lib, "ws2_32.lib") 
namespace {
void LOG_WIN32_ERROR(const char* context)
{
    const DWORD error = GetLastError();
    spdlog::error("{} | code={} ({})", context, error, win32_error_str(error));
}
}

namespace Tunnel {

    HostAddress HostAddress::fromIPv4(uint32_t hostOrder) noexcept
    {
        HostAddress a;
        a.type = Type::IPv4;
        a.data[0] = static_cast<uint8_t>((hostOrder >> 24) & 0xFF);
        a.data[1] = static_cast<uint8_t>((hostOrder >> 16) & 0xFF);
        a.data[2] = static_cast<uint8_t>((hostOrder >> 8) & 0xFF);
        a.data[3] = static_cast<uint8_t>(hostOrder & 0xFF);
        return a;
    }

    HostAddress HostAddress::fromIPv6(const uint8_t bytes[16]) noexcept
    {
        HostAddress a;
        a.type = Type::IPv6;
        std::memcpy(a.data, bytes, 16);
        return a;
    }

    std::string HostAddress::toString() const
    {
        if (type == Type::IPv4) {
            char buf[INET_ADDRSTRLEN]{};
            uint32_t nbo = htonl(
                (static_cast<uint32_t>(data[0]) << 24) |
                (static_cast<uint32_t>(data[1]) << 16) |
                (static_cast<uint32_t>(data[2]) << 8) |
                static_cast<uint32_t>(data[3]));
            ::inet_ntop(AF_INET, &nbo, buf, sizeof(buf));
            return buf;
        }
        if (type == Type::IPv6) {
            char buf[INET6_ADDRSTRLEN]{};
            ::inet_ntop(AF_INET6, data, buf, sizeof(buf));
            return buf;
        }
        return {};
    }

    Key::Key(const std::vector<uint8_t>& bytes)
    {
        if (static_cast<int>(bytes.size()) != KeySize)
            throw std::invalid_argument("Key must be exactly 32 bytes");
        m_bytes = bytes;
    }

    Key::Key(const uint8_t* bytes)
    {
        if (!bytes)
            throw std::invalid_argument("Key: null pointer");
        m_bytes.assign(bytes, bytes + KeySize);
    }

    std::string Key::toBase64() const
    {
        // RFC 4648 base64 table
        static constexpr char kTable[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        const uint8_t* in = m_bytes.data();
        const size_t   len = m_bytes.size();

        std::string out;
        out.reserve(((len + 2) / 3) * 4);

        for (size_t i = 0; i < len; i += 3) {
            const uint32_t b0 = in[i];
            const uint32_t b1 = (i + 1 < len) ? in[i + 1] : 0u;
            const uint32_t b2 = (i + 2 < len) ? in[i + 2] : 0u;
            const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;

            out += kTable[(triple >> 18) & 0x3F];
            out += kTable[(triple >> 12) & 0x3F];
            out += (i + 1 < len) ? kTable[(triple >> 6) & 0x3F] : '=';
            out += (i + 2 < len) ? kTable[triple & 0x3F] : '=';
        }
        return out;
    }

    std::string AllowedIP::toString() const
    {
        return address.toString() + "/" + std::to_string(cidr);
    }

    namespace Driver {

        namespace {

            constexpr const wchar_t* kDllName = L"wireguard.dll";

            template<typename Fn>
            Fn resolveExport(HMODULE hDll, const char* name)
            {
                auto fn = reinterpret_cast<Fn>(::GetProcAddress(hDll, name));
                if (!fn) {
                    LOG_WIN32_ERROR(name);
                    throw std::runtime_error(
                        std::string("wireguard.dll: missing export: ") + name);
                }
                spdlog::debug("wireguard.dll: resolved {}", name);
                return fn;
            }

            std::once_flag g_dllOnce;
            DllFunctions   g_dll;

            void loadDll()
            {
                HMODULE hDll = ::GetModuleHandleW(kDllName);
                if (!hDll)
                    hDll = ::LoadLibraryW(kDllName);
                if (!hDll) {
                    LOG_WIN32_ERROR("LoadLibraryW(wireguard.dll)");
                    throw std::runtime_error("Failed to load wireguard.dll");
                }

                spdlog::info("wireguard.dll loaded (handle={:p})", static_cast<void*>(hDll));

                g_dll.openAdapter = resolveExport<OpenAdapterFn>(hDll, "WireGuardOpenAdapter");
                g_dll.closeAdapter = resolveExport<CloseAdapterFn>(hDll, "WireGuardCloseAdapter");
                g_dll.createAdapter = resolveExport<CreateAdapterFn>(hDll, "WireGuardCreateAdapter");
                g_dll.deleteDriver = resolveExport<DeleteDriverFn>(hDll, "WireGuardDeleteDriver");
                g_dll.getAdapterLUID = resolveExport<GetAdapterLUIDFn>(hDll, "WireGuardGetAdapterLUID");
                g_dll.getAdapterState = resolveExport<GetAdapterStateFn>(hDll, "WireGuardGetAdapterState");
                g_dll.setAdapterState = resolveExport<SetAdapterStateFn>(hDll, "WireGuardSetAdapterState");
                g_dll.getConfiguration = resolveExport<GetConfigurationFn>(hDll, "WireGuardGetConfiguration");
                g_dll.setConfiguration = resolveExport<SetConfigurationFn>(hDll, "WireGuardSetConfiguration");
                g_dll.getRunningDriverVersion = resolveExport<GetRunningDriverVersionFn>(hDll, "WireGuardGetRunningDriverVersion");
                g_dll.setAdapterLogging = resolveExport<SetAdapterLoggingFn>(hDll, "WireGuardSetAdapterLogging");
            }

            // ---- Parsing helpers -------------------------------------------------------

            inline const uint8_t* advance(const uint8_t* ptr, size_t offset,
                const uint8_t* end, const char* tag)
            {
                if (ptr + offset > end)
                    throw std::runtime_error(
                        std::string("GetConfiguration: buffer overrun at ") + tag);
                return ptr + offset;
            }

            /// Converts Windows FILETIME (100-ns ticks since 1601-01-01) to ms since Unix epoch.
            /// Returns 0 if the value is zero or pre-epoch.
            int64_t fileTimeToMsec(uint64_t ft) noexcept
            {
                constexpr uint64_t kEpochDiff100ns = 116'444'736'000'000'000ULL;
                if (ft <= kEpochDiff100ns)
                    return 0;
                return static_cast<int64_t>((ft - kEpochDiff100ns) / 10'000);
            }

            bool parseEndpoint(const SOCKADDR_INET& ep,
                HostAddress& addr, uint16_t& port) noexcept
            {
                if (ep.si_family == AF_INET) {
                    uint32_t nbo{};
                    std::memcpy(&nbo, &ep.Ipv4.sin_addr, sizeof(nbo));
                    addr = HostAddress::fromIPv4(ntohl(nbo));
                    port = ntohs(ep.Ipv4.sin_port);
                    return true;
                }
                if (ep.si_family == AF_INET6) {
                    addr = HostAddress::fromIPv6(ep.Ipv6.sin6_addr.u.Byte);
                    port = ntohs(ep.Ipv6.sin6_port);
                    return true;
                }
                spdlog::warn("parseEndpoint: unknown address family {}", ep.si_family);
                return false;
            }

            /// Narrow helper: converts wstring to UTF-8 string for logging
            std::string wToUtf8(const std::wstring& w)
            {
                if (w.empty()) return {};
                const int sz = ::WideCharToMultiByte(
                    CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                    nullptr, 0, nullptr, nullptr);
                std::string s(sz, '\0');
                ::WideCharToMultiByte(
                    CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                    s.data(), sz, nullptr, nullptr);
                return s;
            }

        } // anonymous namespace

        const DllFunctions& getDll()
        {
            std::call_once(g_dllOnce, loadDll);
            return g_dll;
        }

        std::string getRunningDriverVersion()
        {
            DWORD ver{};
            if (!getDll().getRunningDriverVersion(&ver)) {
                LOG_WIN32_ERROR("WireGuardGetRunningDriverVersion");
                return {};
            }
            return std::to_string((ver >> 24) & 0xFF) + "." +
                std::to_string((ver >> 16) & 0xFF) + "." +
                std::to_string((ver >> 8) & 0xFF) + "." +
                std::to_string(ver & 0xFF);
        }

        Adapter::Adapter(HANDLE h) noexcept
            : m_handle(h)
        {
        }

        Adapter Adapter::open(const std::wstring& name)
        {
            const DllFunctions& dll = getDll();
            HANDLE h = dll.openAdapter(name.c_str());
            if (!h || h == INVALID_HANDLE_VALUE) {
                LOG_WIN32_ERROR("WireGuardOpenAdapter");
                throw std::runtime_error(
                    "WireGuardOpenAdapter(\"" + wToUtf8(name) + "\"): error="
                    + std::to_string(::GetLastError()));
            }
            spdlog::trace("WireGuardOpenAdapter: \"{}\" ok", wToUtf8(name));
            return Adapter{ h };
        }

        Adapter Adapter::create(const std::wstring& name,
            const std::wstring& tunnelType,
            const GUID* guid)
        {
            const DllFunctions& dll = getDll();
            HANDLE h = dll.createAdapter(name.c_str(), tunnelType.c_str(), guid);
            if (!h || h == INVALID_HANDLE_VALUE) {
                LOG_WIN32_ERROR("WireGuardCreateAdapter");
                throw std::runtime_error(
                    "WireGuardCreateAdapter(\"" + wToUtf8(name) + "\"): error="
                    + std::to_string(::GetLastError()));
            }
            spdlog::info("WireGuardCreateAdapter: \"{}\" ok", wToUtf8(name));
            return Adapter{ h };
        }

        Adapter::~Adapter()
        {
            if (m_handle && m_handle != INVALID_HANDLE_VALUE) {
                getDll().closeAdapter(m_handle);
                spdlog::trace("WireGuardCloseAdapter: handle={:p}", m_handle);
            }
        }

        Adapter::Adapter(Adapter&& other) noexcept
            : m_handle(other.m_handle)
            , m_lastGetGuess(other.m_lastGetGuess)
        {
            other.m_handle = nullptr;
        }

        Adapter& Adapter::operator=(Adapter&& other) noexcept
        {
            if (this != &other) {
                if (m_handle && m_handle != INVALID_HANDLE_VALUE)
                    getDll().closeAdapter(m_handle);
                m_handle = other.m_handle;
                m_lastGetGuess = other.m_lastGetGuess;
                other.m_handle = nullptr;
            }
            return *this;
        }

        // ---- Adapter-level operations ----------------------------------------------

        NET_LUID Adapter::luid() const
        {
            NET_LUID l{};
            if (!getDll().getAdapterLUID(m_handle, &l)) {
                LOG_WIN32_ERROR("WireGuardGetAdapterLUID");
                throw std::runtime_error("WireGuardGetAdapterLUID failed");
            }
            return l;
        }

        AdapterState Adapter::state() const
        {
            DWORD s{};
            if (!getDll().getAdapterState(m_handle, &s)) {
                LOG_WIN32_ERROR("WireGuardGetAdapterState");
                throw std::runtime_error("WireGuardGetAdapterState failed");
            }
            return static_cast<AdapterState>(s);
        }

        void Adapter::setState(AdapterState s) const
        {
            if (!getDll().setAdapterState(m_handle, static_cast<DWORD>(s))) {
                LOG_WIN32_ERROR("WireGuardSetAdapterState");
                throw std::runtime_error("WireGuardSetAdapterState failed");
            }
            spdlog::info("Adapter::setState -> {}",
                s == AdapterState::Up ? "Up" : "Down");
        }

        void Adapter::setLogging(DWORD logState) const
        {
            if (!getDll().setAdapterLogging(m_handle, logState)) {
                LOG_WIN32_ERROR("WireGuardSetAdapterLogging");
                throw std::runtime_error("WireGuardSetAdapterLogging failed");
            }
        }

        // ---- setConfiguration ------------------------------------------------------

        void Adapter::setConfiguration(const std::vector<uint8_t>& blob) const
        {
            if (blob.empty())
                throw std::invalid_argument("setConfiguration: blob must not be empty");

            const DWORD sz = static_cast<DWORD>(blob.size());
            if (!getDll().setConfiguration(
                m_handle,
                reinterpret_cast<const BYTE*>(blob.data()),
                sz))
            {
                LOG_WIN32_ERROR("WireGuardSetConfiguration");
                throw std::runtime_error(
                    std::string("WireGuardSetConfiguration failed, error=")
                    + std::to_string(::GetLastError()));
            }
            spdlog::debug("WireGuardSetConfiguration: {} bytes written", sz);
        }

        // ---- getConfiguration ------------------------------------------------------

        Interface Adapter::getConfiguration() const
        {
            static constexpr DWORD kMaxBufSize = 64u * 1024u * 1024u;

            std::vector<uint8_t> buf;

            for (;;) {
                if (m_lastGetGuess == 0)
                    m_lastGetGuess = 4096;

                buf.resize(m_lastGetGuess);
                DWORD needed = m_lastGetGuess;

                const BOOL ok = getDll().getConfiguration(
                    m_handle,
                    reinterpret_cast<BYTE*>(buf.data()),
                    &needed);

                if (ok) {
                    const DWORD used = (needed > 0) ? needed : m_lastGetGuess;
                    m_lastGetGuess = std::max(used, m_lastGetGuess);
                    buf.resize(used);
                    spdlog::trace("WireGuardGetConfiguration: {} bytes", used);
                    break;
                }

                const DWORD err = ::GetLastError();
                if (err != ERROR_MORE_DATA) {
                    LOG_WIN32_ERROR("WireGuardGetConfiguration");
                    throw std::runtime_error(
                        std::string("WireGuardGetConfiguration failed, error=")
                        + std::to_string(err));
                }

                const DWORD next = (needed > m_lastGetGuess) ? needed : m_lastGetGuess * 2;
                if (next > kMaxBufSize) {
                    spdlog::critical("WireGuardGetConfiguration: required buffer ({}) exceeds "
                        "safety limit ({})", next, kMaxBufSize);
                    throw std::runtime_error(
                        "WireGuardGetConfiguration: buffer size limit exceeded");
                }
                spdlog::debug("WireGuardGetConfiguration: growing buffer {} -> {}",
                    m_lastGetGuess, next);
                m_lastGetGuess = next;
            }

            // ---- Parse the flat binary blob ----------------------------------------
            const uint8_t* const bufBegin = buf.data();
            const uint8_t* const bufEnd = bufBegin + buf.size();
            const uint8_t* ptr = bufBegin;

            if (buf.size() < sizeof(IoctlInterface))
                throw std::runtime_error(
                    "GetConfiguration: buffer too small for IoctlInterface header");

            const auto& ioctlIface = *reinterpret_cast<const IoctlInterface*>(ptr);

            Interface iface;

            if (hasFlag(ioctlIface.Flags, IoctlInterfaceFlags::HasPublicKey))
                iface.publicKey = std::make_shared<Key>(ioctlIface.PublicKey);
            if (hasFlag(ioctlIface.Flags, IoctlInterfaceFlags::HasPrivateKey))
                iface.privateKey = std::make_shared<Key>(ioctlIface.PrivateKey);
            if (hasFlag(ioctlIface.Flags, IoctlInterfaceFlags::HasListenPort))
                iface.listenPort = ioctlIface.ListenPort;

            const uint32_t peersCount = ioctlIface.PeersCount;
            iface.peers.reserve(peersCount);
            spdlog::trace("GetConfiguration: parsing {} peer(s)", peersCount);

            ptr = advance(bufBegin, sizeof(IoctlInterface), bufEnd, "first IoctlPeer");
            const auto* ioctlPeer = reinterpret_cast<const IoctlPeer*>(ptr);

            for (uint32_t i = 0; i < peersCount; ++i)
            {
                advance(reinterpret_cast<const uint8_t*>(ioctlPeer),
                    sizeof(IoctlPeer), bufEnd, "IoctlPeer header");

                Peer peer;

                if (hasFlag(ioctlPeer->Flags, IoctlPeerFlags::HasPublicKey))
                    peer.publicKey = std::make_shared<Key>(ioctlPeer->PublicKey);
                if (hasFlag(ioctlPeer->Flags, IoctlPeerFlags::HasPresharedKey))
                    peer.presharedKey = std::make_shared<Key>(ioctlPeer->PresharedKey);
                if (hasFlag(ioctlPeer->Flags, IoctlPeerFlags::HasPersistentKeepalive))
                    peer.persistentKeepalive = ioctlPeer->PersistentKeepalive;

                if (hasFlag(ioctlPeer->Flags, IoctlPeerFlags::HasEndpoint)) {
                    if (!parseEndpoint(ioctlPeer->Endpoint,
                        peer.endpointAddress, peer.endpointPort))
                    {
                        spdlog::warn("GetConfiguration: peer[{}] unsupported endpoint family", i);
                    }
                }

                peer.txBytes = ioctlPeer->TxBytes;
                peer.rxBytes = ioctlPeer->RxBytes;
                peer.lastHandshakeMsec = fileTimeToMsec(ioctlPeer->LastHandshake);

                // ---- AllowedIPs ------------------------------------------------
                const uint32_t allowedCount = ioctlPeer->AllowedIPsCount;
                peer.allowedIPs.reserve(allowedCount);

                const auto* ioctlAIP = reinterpret_cast<const IoctlAllowedIP*>(
                    advance(reinterpret_cast<const uint8_t*>(ioctlPeer),
                        sizeof(IoctlPeer), bufEnd, "first IoctlAllowedIP"));

                for (uint32_t j = 0; j < allowedCount; ++j)
                {
                    advance(reinterpret_cast<const uint8_t*>(ioctlAIP),
                        sizeof(IoctlAllowedIP), bufEnd, "IoctlAllowedIP");

                    AllowedIP aip;
                    aip.family = ioctlAIP->AddressFamily;
                    aip.cidr = ioctlAIP->Cidr;

                    if (ioctlAIP->AddressFamily == AF_INET) {
                        uint32_t nbo{};
                        std::memcpy(&nbo, &ioctlAIP->V4, sizeof(nbo));
                        aip.address = HostAddress::fromIPv4(ntohl(nbo));
                    }
                    else if (ioctlAIP->AddressFamily == AF_INET6) {
                        aip.address = HostAddress::fromIPv6(
                            reinterpret_cast<const uint8_t*>(&ioctlAIP->V6));
                    }
                    else {
                        spdlog::warn("GetConfiguration: peer[{}] allowedIP[{}] "
                            "unknown family {}", i, j, ioctlAIP->AddressFamily);
                    }

                    peer.allowedIPs.push_back(aip);
                    ++ioctlAIP;
                }

                spdlog::trace("GetConfiguration: peer[{}] tx={} rx={} aips={}",
                    i, peer.txBytes, peer.rxBytes, peer.allowedIPs.size());

                iface.peers.push_back(std::move(peer));
                ioctlPeer = reinterpret_cast<const IoctlPeer*>(ioctlAIP);
            }

            return iface;
        }

    } // namespace Driver
} // namespace Tunnel
