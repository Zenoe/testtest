/*
 * Windows	Credential Manager  control /name Microsoft.CredentialManager
 * macOS	Keychain
 * Linux	Secret Service / libsecret
 */
#pragma once
#include <QString>
#include <optional>

class ISecureStorage {
public:
    virtual ~ISecureStorage() = default;

    virtual bool save(const QString& key,
                      const QString& username,
                      const QString& secret,
                      QString& error) = 0;

    virtual std::optional<QString> load(const QString& key,
                                        QString& error) = 0;

    virtual bool remove(const QString& key,
                        QString& error) = 0;
};
