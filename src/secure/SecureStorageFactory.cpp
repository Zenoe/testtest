#include "SecureStorageFactory.h"

#ifdef Q_OS_WIN
#include "WinCredentialStorage.h"
#endif

std::unique_ptr<ISecureStorage> SecureStorageFactory::create()
{
#ifdef Q_OS_WIN
    return std::make_unique<WinCredentialStorage>();
#else
    return nullptr; // later: macOS Keychain / libsecret
#endif
}