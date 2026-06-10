#pragma once
// ============================================================
//  Driver.h  –  WireGuard wireguard.dll C++ wrapper
//  Targets: Windows 10+, spdlog  (no Qt)
// ============================================================

#include "WireGuardTypes.h"

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

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace Tunnel {

    // ============================================================
    //  HostAddress  –  lightweight IPv4/IPv6 holder
    // ============================================================
    struct HostAddress
    {
        enum class Type { None, IPv4, IPv6 };

        Type    type = Type::None;
        uint8_t data[16]{};   ///< IPv4 in first 4 bytes (host order); IPv6 in all 16

        HostAddress() = default;

        /// Construct from IPv4 host-byte-order uint32
        static HostAddress fromIPv4(uint32_t hostOrder) noexcept;

        /// Construct from raw 16-byte IPv6 address
        static HostAddress fromIPv6(const uint8_t bytes[16]) noexcept;

        /// "a.b.c.d" or "xxxx:...:xxxx" or ""
        [[nodiscard]] std::string toString() const;

        [[nodiscard]] bool isNull() const noexcept { return type == Type::None; }
    };

    // ============================================================
    //  Key  –  immutable 32-byte WireGuard key
    // ============================================================
    class Key
    {
    public:
        static constexpr int KeySize = 32;

        /// Throws std::invalid_argument if bytes.size() != KeySize.
        explicit Key(const std::vector<uint8_t>& bytes);

        /// Throws std::invalid_argument if bytes is null.
        explicit Key(const uint8_t* bytes);

        [[nodiscard]] const std::vector<uint8_t>& bytes() const noexcept { return m_bytes; }

        /// Standard base64 (RFC 4648)
        [[nodiscard]] std::string toBase64() const;

    private:
        std::vector<uint8_t> m_bytes;
    };

    // ============================================================
    //  AllowedIP
    // ============================================================
    struct AllowedIP
    {
        HostAddress    address;
        uint8_t        cidr = 0;
        ADDRESS_FAMILY family = AF_UNSPEC;

        [[nodiscard]] std::string toString() const;   ///< "10.0.0.0/8"
    };

    // ============================================================
    //  Peer / Interface
    // ============================================================
    struct Peer
    {
        std::shared_ptr<Key> publicKey;
        std::shared_ptr<Key> presharedKey;
        uint16_t             persistentKeepalive = 0;
        HostAddress          endpointAddress;
        uint16_t             endpointPort = 0;
        uint64_t             txBytes = 0;
        uint64_t             rxBytes = 0;

        /// Milliseconds since Unix epoch (UTC); 0 = never
        int64_t              lastHandshakeMsec = 0;

        std::vector<AllowedIP> allowedIPs;
    };

    struct Interface
    {
        uint16_t             listenPort = 0;
        std::shared_ptr<Key> privateKey;
        std::shared_ptr<Key> publicKey;
        std::vector<Peer>    peers;
    };

    // ============================================================
    //  namespace Driver  –  wireguard.dll bindings
    // ============================================================
    namespace Driver {

        // ---- wireguard.dll function pointer typedefs ----

        using OpenAdapterFn = HANDLE(WINAPI*)(LPCWSTR name);
        using CloseAdapterFn = VOID(WINAPI*)(HANDLE adapter);
        using CreateAdapterFn = HANDLE(WINAPI*)(LPCWSTR name, LPCWSTR tunnelType,
            const GUID* requestedGUID);
        using DeleteDriverFn = BOOL(WINAPI*)();
        using GetAdapterLUIDFn = BOOL(WINAPI*)(HANDLE adapter, NET_LUID* luid);
        using GetAdapterStateFn = BOOL(WINAPI*)(HANDLE adapter, DWORD* state);
        using SetAdapterStateFn = BOOL(WINAPI*)(HANDLE adapter, DWORD state);
        using GetConfigurationFn = BOOL(WINAPI*)(HANDLE adapter, BYTE* iface, DWORD* bytes);
        using SetConfigurationFn = BOOL(WINAPI*)(HANDLE adapter,
            const BYTE* iface, DWORD bytes);
        using GetRunningDriverVersionFn = BOOL(WINAPI*)(DWORD* version);
        using SetAdapterLoggingFn = BOOL(WINAPI*)(HANDLE adapter, DWORD logState);

        // ---- Adapter state constants ----
        enum class AdapterState : DWORD {
            Down = 0,
            Up = 1,
        };

        // ============================================================
        //  DllFunctions
        // ============================================================
        struct DllFunctions
        {
            OpenAdapterFn             openAdapter = nullptr;
            CloseAdapterFn            closeAdapter = nullptr;
            CreateAdapterFn           createAdapter = nullptr;
            DeleteDriverFn            deleteDriver = nullptr;
            GetAdapterLUIDFn          getAdapterLUID = nullptr;
            GetAdapterStateFn         getAdapterState = nullptr;
            SetAdapterStateFn         setAdapterState = nullptr;
            GetConfigurationFn        getConfiguration = nullptr;
            SetConfigurationFn        setConfiguration = nullptr;
            GetRunningDriverVersionFn getRunningDriverVersion = nullptr;
            SetAdapterLoggingFn       setAdapterLogging = nullptr;
        };

        /// Returns the process-wide resolved DLL function table.
        /// Throws std::runtime_error on the first failed load/resolve.
        [[nodiscard]] const DllFunctions& getDll();

        /// Returns the running wireguard.dll driver version as "major.minor.patch.build",
        /// or an empty string if the driver is not running.
        [[nodiscard]] std::string getRunningDriverVersion();

        // ============================================================
        //  Adapter  –  RAII wrapper around a WireGuard adapter handle
        // ============================================================
        class Adapter
        {
        public:
            // ---- Construction / destruction ----------------------------------------

            /// Opens an existing adapter by name.
            /// Throws std::runtime_error on failure.
            [[nodiscard]] static Adapter open(const std::wstring& name);

            /// Creates a new adapter.
            /// Throws std::runtime_error on failure.
            [[nodiscard]] static Adapter create(const std::wstring& name,
                const std::wstring& tunnelType = L"WireGuard",
                const GUID* guid = nullptr);

            ~Adapter();

            Adapter(const Adapter&) = delete;
            Adapter& operator=(const Adapter&) = delete;
            Adapter(Adapter&&)                 noexcept;
            Adapter& operator=(Adapter&&)      noexcept;

            // ---- Adapter-level operations ------------------------------------------

            [[nodiscard]] NET_LUID     luid()  const;
            [[nodiscard]] AdapterState state() const;

            void setState(AdapterState s)    const;
            void setLogging(DWORD logState)  const;

            // ---- Configuration -----------------------------------------------------

            /// Reads the current configuration from the kernel driver.
            [[nodiscard]] Interface getConfiguration() const;

            /// Writes a complete configuration blob to the kernel driver.
            void setConfiguration(const std::vector<uint8_t>& blob) const;

            // ---- Raw handle --------------------------------------------------------
            [[nodiscard]] HANDLE handle() const noexcept { return m_handle; }

        private:
            explicit Adapter(HANDLE h) noexcept;

            HANDLE        m_handle = nullptr;
            mutable DWORD m_lastGetGuess = 4096;
        };

    } // namespace Driver
} // namespace Tunnel