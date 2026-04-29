// Wraps an SC_HANDLE so it is always closed on scope exit.
struct ScHandle {
    SC_HANDLE h = nullptr;
    explicit ScHandle(SC_HANDLE h) : h(h) {}
    ~ScHandle() { if (h) CloseServiceHandle(h); }
    operator SC_HANDLE() const { return h; }
    bool valid()  const { return h != nullptr; }
    ScHandle(const ScHandle&) = delete;
    ScHandle& operator=(const ScHandle&) = delete;
};


// Wraps a Win32 HANDLE (events, threads …) so it is always closed on scope exit.
struct WinHandle {
    HANDLE h = INVALID_HANDLE_VALUE;
    explicit WinHandle(HANDLE h = INVALID_HANDLE_VALUE) : h(h) {}
    ~WinHandle() { close(); }
    void close() { if (h && h != INVALID_HANDLE_VALUE) { CloseHandle(h); h = INVALID_HANDLE_VALUE; } }
    operator HANDLE() const { return h; }
    bool valid() const { return h && h != INVALID_HANDLE_VALUE; }
    WinHandle(const WinHandle&) = delete;
    WinHandle& operator=(const WinHandle&) = delete;
};
