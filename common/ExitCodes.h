#pragma once

namespace XyreExitCode {
    enum Code : int {
        // ── Success ───────────────────────────────────────────
        Ok                  = 0,
        AlreadyExists       = 1,   // add: service already registered
        AlreadyRunning      = 2,   // start: service already in SERVICE_RUNNING state
        AlreadyStopped      = 3,   // stop: service already stopped / not running
        NotFound            = 4,   // start/stop/uninstall: service doesn't exist

        // ── Hard failures ─────────────────────────────────────
        InvalidArguments    = 10,
        AccessDenied        = 11,  // SCM permission error
        ServiceCreateFailed = 12,
        ServiceStartFailed  = 13,
        ServiceStopFailed   = 14,
        ServiceRemoveFailed = 15,
        UnknownCommand      = 16,
        UnexpectedError     = 99,
    };

    // Codes that are "soft" — caller should treat as success or handle gracefully
    inline bool isSoftCode(int code) {
        return code == Ok
            || code == AlreadyExists
            || code == AlreadyRunning
            || code == AlreadyStopped
            || code == NotFound;
    }
}
