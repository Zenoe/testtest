#include "WinCredentialStorage.h"
#include <windows.h>
#include <wincred.h>

bool WinCredentialStorage::save(const QString& key,
    const QString& username,
    const QString& secret,
    QString& error)
{
    std::wstring target = key.toStdWString();
    std::wstring user = username.toStdWString();
    std::wstring token = secret.toStdWString();

    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPWSTR>(target.c_str());
    cred.UserName = const_cast<LPWSTR>(user.c_str());

    cred.CredentialBlobSize =
        static_cast<DWORD>(token.size() * sizeof(wchar_t));
    cred.CredentialBlob =
        reinterpret_cast<LPBYTE>(const_cast<wchar_t*>(token.c_str()));

    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    if (!CredWriteW(&cred, 0)) {
        error = QString("CredWrite failed: %1").arg(GetLastError());
        return false;
    }

    return true;
}

std::optional<QString> WinCredentialStorage::load(const QString& key,
    QString& error)
{
    std::wstring target = key.toStdWString();
    PCREDENTIALW pcred = nullptr;

    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &pcred)) {
        error = QString("CredRead failed: %1").arg(GetLastError());
        return std::nullopt;
    }

    QString result = QString::fromWCharArray(
        reinterpret_cast<wchar_t*>(pcred->CredentialBlob),
        pcred->CredentialBlobSize / sizeof(wchar_t)
    );

    CredFree(pcred);
    return result;
}

bool WinCredentialStorage::remove(const QString& key,
    QString& error)
{
    std::wstring target = key.toStdWString();

    if (!CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0)) {
        error = QString("CredDelete failed: %1").arg(GetLastError());
        return false;
    }

    return true;
}
