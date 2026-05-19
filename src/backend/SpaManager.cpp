#include "SpaManager.h"
#include "utils/ConfigManager.h"   // your existing singleton

#include <QDateTime>
#include <QHostAddress>
#include <QRandomGenerator>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include <spdlog/spdlog.h>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

    /// Drain the OpenSSL error queue into a human-readable string.
    QString opensslError() {
        char buf[256];
        ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
        return QString::fromLatin1(buf);
    }

    /// Decode a hex or base64 string from config into raw bytes.
    /// Prefers hex (64 chars = 32 bytes); falls back to base64.
    QByteArray decodeKey(const std::string& s) {
        const auto qs = QString::fromStdString(s).trimmed();
        if (qs.length() == 64) {                        // 32-byte hex
            return QByteArray::fromHex(qs.toLatin1());
        }
        return QByteArray::fromBase64(qs.toLatin1());   // base64
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
SpaManager::SpaManager(QObject* parent) : QObject(parent) {
    m_delayTimer.setSingleShot(true);
    connect(&m_delayTimer, &QTimer::timeout, this, &SpaManager::ready);
}

// ─────────────────────────────────────────────────────────────────────────────
bool SpaManager::loadFromConfig() {
    auto& cm = ConfigManager::instance();

    // spa.enabled — if false, SPA is bypassed entirely (emit ready immediately)
    const bool enabled = cm.get<bool>("spa.enabled").value_or(false);
    if (!enabled) {
        spdlog::info("SpaManager: SPA disabled in config, knock is a no-op");
        m_cfg = {};   // empty aesKey → knock() fast-paths
        return true;
    }

    Config c;
    // Reuse the same host as the HTTPS server
    c.serverHost = QString::fromStdString(
        cm.get<std::string>("server.host").value_or(""));
    c.spaPort = static_cast<quint16>(
        cm.get<int>("spa.port").value_or(62201));
    c.accessRequest = QString::fromStdString(
        cm.get<std::string>("spa.access_request")
        .value_or("0.0.0.0/0,tcp/443"));
    c.postKnockDelayMs = cm.get<int>("spa.delay_ms").value_or(500);

    const auto aesHex = cm.get<std::string>("spa.aes_key");
    const auto hmacHex = cm.get<std::string>("spa.hmac_key");

    if (!aesHex || !hmacHex) {
        spdlog::error("SpaManager: spa.aes_key / spa.hmac_key missing in config");
        return false;
    }

    c.aesKey = decodeKey(*aesHex);
    c.hmacKey = decodeKey(*hmacHex);

    if (c.aesKey.size() != 32 || c.hmacKey.size() != 32) {
        spdlog::error("SpaManager: keys must decode to exactly 32 bytes each "
            "(got aes={} hmac={})", c.aesKey.size(), c.hmacKey.size());
        return false;
    }

    if (c.serverHost.isEmpty()) {
        spdlog::error("SpaManager: server.host is empty");
        return false;
    }

    m_cfg = std::move(c);
    spdlog::info("SpaManager: config loaded — host={} port={}",
        m_cfg.serverHost.toStdString(), m_cfg.spaPort);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
void SpaManager::knock(const QString& username) {
    m_cfg.username = username;

    // Fast-path: SPA not configured → behave as if knock succeeded immediately
    if (m_cfg.aesKey.size() != 32) {
        spdlog::debug("SpaManager: SPA not configured, skipping knock");
        emit ready();
        return;
    }

    spdlog::info("SpaManager: sending knock → {}:{} for user='{}'",
        m_cfg.serverHost.toStdString(), m_cfg.spaPort,
        username.toStdString());

    QByteArray packet;
    try {
        packet = buildWirePacket();
    }
    catch (const std::exception& ex) {
        const QString err = QString("SPA packet build failed: %1").arg(ex.what());
        spdlog::error("SpaManager: {}", err.toStdString());
        emit failed(err);
        return;
    }

    const qint64 sent = m_socket.writeDatagram(
        packet, QHostAddress(m_cfg.serverHost), m_cfg.spaPort);

    if (sent != packet.size()) {
        const QString err = QString("UDP send failed: %1").arg(m_socket.errorString());
        spdlog::error("SpaManager: {}", err.toStdString());
        emit failed(err);
        return;
    }

    spdlog::debug("SpaManager: knock sent ({} bytes), waiting {}ms",
        packet.size(), m_cfg.postKnockDelayMs);

    // Wait for firewall rule to propagate, then signal the login to proceed
    m_delayTimer.start(m_cfg.postKnockDelayMs);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Packet internals
// ─────────────────────────────────────────────────────────────────────────────

QByteArray SpaManager::buildPlainPayload() const {
    // fwknop v2 plaintext: "user:epochSeconds:version:mode:accessRequest:digest"
    // mode 1 = ACCESS request
    const QString ts = QString::number(QDateTime::currentSecsSinceEpoch());
    const QString msg = QStringLiteral("%1:%2:2.6.11:1:%3:0")
        .arg(m_cfg.username, ts, m_cfg.accessRequest);
    return msg.toUtf8();
}

QByteArray SpaManager::aesCbcEncrypt(const QByteArray& plain,
    const QByteArray& key,
    QByteArray& ivOut) {
    // Generate a fresh random IV for every packet (replay resistance)
    ivOut.resize(16);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(ivOut.data()), 16) != 1)
        throw std::runtime_error("RAND_bytes failed: " + opensslError().toStdString());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    struct CtxGuard {
        EVP_CIPHER_CTX* p;
        ~CtxGuard() { EVP_CIPHER_CTX_free(p); }
    } guard{ ctx };

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
        reinterpret_cast<const unsigned char*>(key.constData()),
        reinterpret_cast<const unsigned char*>(ivOut.constData())) != 1)
        throw std::runtime_error("EVP_EncryptInit_ex: " + opensslError().toStdString());

    // Output buffer: plaintext size + up to one full block of padding
    QByteArray cipher(plain.size() + EVP_MAX_BLOCK_LENGTH, '\0');
    int len1 = 0, len2 = 0;

    if (EVP_EncryptUpdate(ctx,
        reinterpret_cast<unsigned char*>(cipher.data()), &len1,
        reinterpret_cast<const unsigned char*>(plain.constData()),
        static_cast<int>(plain.size())) != 1)
        throw std::runtime_error("EVP_EncryptUpdate: " + opensslError().toStdString());

    if (EVP_EncryptFinal_ex(ctx,
        reinterpret_cast<unsigned char*>(cipher.data()) + len1, &len2) != 1)
        throw std::runtime_error("EVP_EncryptFinal_ex: " + opensslError().toStdString());

    cipher.resize(len1 + len2);
    return cipher;
}

QByteArray SpaManager::hmacSha256(const QByteArray& data, const QByteArray& key) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  digestLen = 0;

    if (!HMAC(EVP_sha256(),
        key.constData(), static_cast<int>(key.size()),
        reinterpret_cast<const unsigned char*>(data.constData()),
        static_cast<size_t>(data.size()),
        digest, &digestLen))
        throw std::runtime_error("HMAC failed: " + opensslError().toStdString());

    return QByteArray(reinterpret_cast<char*>(digest),
        static_cast<int>(digestLen));
}

QByteArray SpaManager::buildWirePacket() const {
    // 1. Encrypt payload
    QByteArray iv;
    const QByteArray plain = buildPlainPayload();
    const QByteArray ciphertext = aesCbcEncrypt(plain, m_cfg.aesKey, iv);

    // 2. Wire format: Base64(iv ∥ ciphertext)  ∥  HMAC-SHA256(raw_bytes, hmacKey)
    //    The HMAC covers the pre-base64 bytes so the server can verify without
    //    a decryption attempt — same convention as fwknopd --digest-type SHA256
    const QByteArray rawBytes = iv + ciphertext;
    const QByteArray mac = hmacSha256(rawBytes, m_cfg.hmacKey);
    return rawBytes.toBase64() + mac;          // UDP payload
}