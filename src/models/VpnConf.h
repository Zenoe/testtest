#pragma once
struct VpnIfaceConfig {
    QString privateKey;
    QString address;
    QString dns;
    int     mtu = 1420;   // WireGuard recommended default
};

struct VpnPeerConfig {
    QString     endpoint;
    QString     publicKey;
    QString     presharedKey;
    QStringList allowedIPs;
    int         persistentKeepalive = 25; // seconds, 0 = disabled
};

struct VpnConfig {
    VpnPeerConfig  peer;
    VpnIfaceConfig iface;
    QString generateWireguardConfig() const;
    bool writeVpnConfig(const QString& filePath) const;

};