#pragma once
#include <QString>
#include <Windows.h>

struct ServiceResult {
    enum class Status {
        Ok,
        AlreadyExists,    // add:       ERROR_SERVICE_EXISTS
        AlreadyRunning,   // start:     ERROR_SERVICE_ALREADY_RUNNING
        AlreadyStopped,   // stop:      ERROR_SERVICE_NOT_ACTIVE
        NotFound,         // any:       ERROR_SERVICE_DOES_NOT_EXIST
        AccessDenied,     //            ERROR_ACCESS_DENIED
        Failed,           // hard/unexpected Win32 error
    };

    Status  status  = Status::Ok;
    DWORD   win32   = 0;          // raw GetLastError() — 0 if Ok
    QString detail;               // human-readable, logged by wmain

    bool ok()   const { return status == Status::Ok; }

    // "soft" = expected state, caller should continue / treat as success
    bool soft() const {
        return status == Status::Ok
            || status == Status::AlreadyExists
            || status == Status::AlreadyRunning
            || status == Status::AlreadyStopped
            || status == Status::NotFound;
    }

    static ServiceResult success() {
        return { Status::Ok, 0, {} };
    }
    static ServiceResult fromWin32(DWORD err, const QString& ctx) {
        // Map known soft codes first, fall back to hard failure
        switch (err) {
            case ERROR_SERVICE_EXISTS:           return { Status::AlreadyExists,   err, ctx };
            case ERROR_SERVICE_ALREADY_RUNNING:  return { Status::AlreadyRunning,  err, ctx };
            case ERROR_SERVICE_NOT_ACTIVE:       return { Status::AlreadyStopped,  err, ctx };
            case ERROR_SERVICE_DOES_NOT_EXIST:   return { Status::NotFound,        err, ctx };
            case ERROR_ACCESS_DENIED:            return { Status::AccessDenied,    err, ctx };
            default:                             return { Status::Failed,          err, ctx };
        }
    }
};
