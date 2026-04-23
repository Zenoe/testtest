#pragma once
#include "ISecureStorage.h"

class WinCredentialStorage : public ISecureStorage {
public:
    bool save(const QString& key,
        const QString& username,
        const QString& secret,
        QString& error) override;

    std::optional<QString> load(const QString& key,
        QString& error) override;

    bool remove(const QString& key,
        QString& error) override;
};