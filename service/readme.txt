xyreService exit code          isSoftCode?   VpnManager action
─────────────────────────────────────────────────────────────────
Ok (0)                         ✅ yes        continue
AlreadyExists (1)  [add]       ✅ yes        log + skip to start
AlreadyRunning (2) [start]     ✅ yes        log + emit connected
AlreadyStopped (3) [stop]      ✅ yes        log + continue to uninstall
NotFound (4)       [stop]      ✅ yes        log + continue to uninstall
NotFound (4)       [uninstall] ✅ yes        log + emit disconnected
AccessDenied (11)              ❌ no         emit errorOccurred → QMessageBox
ServiceStartFailed (13)        ❌ no         emit errorOccurred → QMessageBox
UnexpectedError (99)           ❌ no         emit errorOccurred → QMessageBox
