#pragma once
#include <memory>
#include "ISecureStorage.h"

class SecureStorageFactory {
public:
    static std::unique_ptr<ISecureStorage> create();
};