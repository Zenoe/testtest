#pragma once

#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_

#include <winsock2.h>
#include <ws2tcpip.h>
#include <ws2ipdef.h>

#include <cstdint>
 // the structs actually used by Driver / Service
#pragma pack(push, 8)

// IoctlInterface flags
enum class IoctlInterfaceFlags : uint32_t {
    None = 0,
    HasPublicKey = 1 << 0,
    HasPrivateKey = 1 << 1,
    HasListenPort = 1 << 2,
    ReplacePeers = 1 << 3,
};
inline IoctlInterfaceFlags operator|(IoctlInterfaceFlags a, IoctlInterfaceFlags b)
{
    return static_cast<IoctlInterfaceFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline IoctlInterfaceFlags operator&(IoctlInterfaceFlags a, IoctlInterfaceFlags b)
{
    return static_cast<IoctlInterfaceFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool hasFlag(IoctlInterfaceFlags v, IoctlInterfaceFlags f)
{
    return (v & f) != IoctlInterfaceFlags::None;
}

// IoctlPeer flags
enum class IoctlPeerFlags : uint32_t {
    None = 0,
    HasPublicKey = 1 << 0,
    HasPresharedKey = 1 << 1,
    HasPersistentKeepalive = 1 << 2,
    HasEndpoint = 1 << 3,
    ReplaceAllowedIPs = 1 << 5,
    Remove = 1 << 6,
    UpdateOnly = 1 << 7,
};
inline IoctlPeerFlags operator|(IoctlPeerFlags a, IoctlPeerFlags b)
{
    return static_cast<IoctlPeerFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline IoctlPeerFlags operator&(IoctlPeerFlags a, IoctlPeerFlags b)
{
    return static_cast<IoctlPeerFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool hasFlag(IoctlPeerFlags v, IoctlPeerFlags f)
{
    return (v & f) != IoctlPeerFlags::None;
}

// Raw IOCTL structs (must match the ABI layout in tunnel.dll / wireguard.dll)
// These mirror the [StructLayout] declarations in Driver.cs exactly.

struct IoctlInterface {                     // Pack=8, Size=80
    IoctlInterfaceFlags Flags;              // 4
    uint16_t            ListenPort;         // 2
    uint8_t             _pad0[2];           // 2  (alignment)
    uint8_t             PrivateKey[32];     // 32
    uint8_t             PublicKey[32];      // 32
    uint32_t            PeersCount;         // 4
    uint8_t             _pad1[4];           // 4  (to reach 80 bytes)
};
static_assert(sizeof(IoctlInterface) == 80, "IoctlInterface size mismatch");

struct IoctlPeer {                          // Pack=8, Size=136
    IoctlPeerFlags      Flags;              // 4
    uint32_t            Reserved;           // 4
    uint8_t             PublicKey[32];      // 32
    uint8_t             PresharedKey[32];   // 32
    uint16_t            PersistentKeepalive;// 2
    uint8_t             _pad0[6];           // 6  (align Endpoint to 8)
    SOCKADDR_INET       Endpoint;           // 28 (sizeof SOCKADDR_INET on Windows)
    uint8_t             _pad1[4];           // align to 8
    uint64_t            TxBytes;            // 8
    uint64_t            RxBytes;            // 8
    uint64_t            LastHandshake;      // 8  (FILETIME-style 100-ns ticks since 1601)
    uint32_t            AllowedIPsCount;    // 4
    uint8_t             _pad2[4];           // 4
};
static_assert(sizeof(IoctlPeer) == 144, "IoctlPeer size mismatch");

struct IoctlAllowedIP {                     // Pack=8, Size=24
    union {
        IN_ADDR  V4;                        // 4 bytes (at offset 0)
        IN6_ADDR V6;                        // 16 bytes (at offset 0)
    };
    uint8_t             _pad[12 - sizeof(IN_ADDR)]; // pad union to 16
    ADDRESS_FAMILY      AddressFamily;      // 2  (at offset 16)
    uint8_t             _pad2[2];
    uint8_t             Cidr;               // 1  (at offset 20)
    uint8_t             _pad3[3];
};
static_assert(sizeof(IoctlAllowedIP) == 32, "IoctlAllowedIP size mismatch");

#pragma pack(pop)
