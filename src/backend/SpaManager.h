#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  SpaManager — Single Packet Authorization (fwknop-compatible)
//  Sends one encrypted/HMAC-signed UDP packet to "dark" the server port open
//  before the HTTPS login attempt.
//
//  Thread-safety: construct and use from the Qt main thread only.
//  All crypto is OpenSSL 3.x (AES-256-CBC + HMAC-SHA256).
// ─────────────────────────────────────────────────────────────────────────────
#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QString>
#include <QByteArray>
#include <optional>

class SpaManager final : public QObject {
    Q_OBJECT

public:
    // ── Configuration ────────────────────────────────────────────────────────
    struct Config {
        QString   serverHost;                  // same host as HTTPS server
        quint16   spaPort = 62201;      // fwknopd default UDP port
        QString   accessRequest;               // e.g. "0.0.0.0/0,tcp/443"
        QByteArray aesKey;                     // exactly 32 bytes
        QByteArray hmacKey;                    // exactly 32 bytes
        QString   username;                    // propagated into SPA payload
        int       postKnockDelayMs = 500;      // wait for fw rule propagation
    };

    explicit SpaManager(QObject* parent = nullptr);
    ~SpaManager() override = default;

    // Non-copyable / non-movable (holds QUdpSocket)
    SpaManager(const SpaManager&) = delete;
    SpaManager& operator=(const SpaManager&) = delete;

    // ── Public API ───────────────────────────────────────────────────────────
    // Load config from ConfigManager (reads spa.* keys).
    // Returns false + logs if keys are missing or malformed.
    bool loadFromConfig();

    // Explicitly override config (e.g. from settings dialog).
    void setConfig(const Config& cfg) { m_cfg = cfg; }
    const Config& config() const { return m_cfg; }

    // Send the knock, then emit ready() after postKnockDelayMs.
    // If SPA is disabled (empty aesKey) emit ready() immediately.
    void knock(const QString& username);

signals:
    // Emitted when the port should be open and HTTPS can proceed.
    void ready();
    // Emitted on crypto / socket error — login should surface this.
    void failed(const QString& reason);

private:
    // ── Packet construction ───────────────────────────────────────────────────
    [[nodiscard]] QByteArray buildPlainPayload() const;
    [[nodiscard]] static QByteArray aesCbcEncrypt(const QByteArray& plain,
        const QByteArray& key,
        QByteArray& ivOut);
    [[nodiscard]] static QByteArray hmacSha256(const QByteArray& data,
        const QByteArray& key);
    [[nodiscard]] QByteArray buildWirePacket() const;   // throws on error

    Config      m_cfg;
    QUdpSocket  m_socket;
    QTimer      m_delayTimer;
};