#pragma once

#include "ServiceResult.h"

class ControllerService {
public:
    static int runDispatcher();
    static ServiceResult install(const QString& exePath);
};
