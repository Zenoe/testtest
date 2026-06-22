#include "ControlServiceClient.h"

#include "ControlProtocol.h"
#include "ExitCodes.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>

#include <Windows.h>
#include <shellapi.h>

#include <algorithm>
#include <string>

namespace {

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}
    ~ScopedHandle() {
        if (m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE)
            CloseHandle(m_handle);
    }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            if (m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE)
                CloseHandle(m_handle);
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    HANDLE get() const { return m_handle; }
    explicit operator bool() const {
        return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE;
    }

private:
    HANDLE m_handle;
};

class ScopedComApartment {
public:
    ScopedComApartment()
        : m_result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}
    ~ScopedComApartment() {
        if (SUCCEEDED(m_result))
            CoUninitialize();
    }

private:
    HRESULT m_result;
};

ControlResponse errorResponse(const QString& detail, int code = XyreExitCode::UnexpectedError,
                              DWORD win32 = 0) {
    return { false, code, win32, detail, {}, {} };
}

ControlResponse parseResponse(const QByteArray& bytes) {
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return errorResponse(QStringLiteral("controller returned an invalid response"));

    const QJsonObject response = document.object();
    if (response.value("version").toInt() != XyreControl::kProtocolVersion)
        return errorResponse(QStringLiteral("controller protocol version mismatch"));

    ControlResponse result{
        response.value("ok").toBool(),
        response.value("code").toInt(XyreExitCode::UnexpectedError),
        static_cast<DWORD>(response.value("win32").toInteger()),
        response.value("detail").toString(),
        response.value("serviceName").toString(),
        response
    };
    return result;
}

bool isTransientPipeError(DWORD error) {
    return error == ERROR_FILE_NOT_FOUND
        || error == ERROR_PIPE_BUSY
        || error == ERROR_PIPE_NOT_CONNECTED
        || error == ERROR_NO_DATA;
}

ControlResponse startInstalledController() {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        const DWORD error = GetLastError();
        return errorResponse(
            QStringLiteral("failed to open Service Control Manager (win32=%1)").arg(error),
            XyreExitCode::ServiceStartFailed, error);
    }

    SC_HANDLE service = OpenServiceW(
        scm,
        XyreControl::kControllerServiceName,
        SERVICE_START | SERVICE_QUERY_STATUS);
    if (!service) {
        const DWORD error = GetLastError();
        CloseServiceHandle(scm);
        return errorResponse(
            QStringLiteral("failed to open installed controller service (win32=%1)").arg(error),
            XyreExitCode::ServiceStartFailed, error);
    }

    SERVICE_STATUS_PROCESS status{};
    DWORD bytesNeeded = 0;
    bool ok = QueryServiceStatusEx(
        service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status),
        sizeof(status), &bytesNeeded);
    DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    if (ok && status.dwCurrentState != SERVICE_RUNNING) {
        ok = StartServiceW(service, 0, nullptr) != FALSE;
        if (!ok) {
            error = GetLastError();
            ok = error == ERROR_SERVICE_ALREADY_RUNNING;
        }
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    if (!ok) {
        return errorResponse(
            QStringLiteral("failed to start installed controller service (win32=%1)").arg(error),
            XyreExitCode::ServiceStartFailed, error);
    }
    return { true, XyreExitCode::Ok, 0, {}, {}, {} };
}

} // namespace

bool ControlServiceClient::isControllerInstalled() {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm)
        return true; // Unknown: do not trigger an unexpected elevation prompt.
    SC_HANDLE service = OpenServiceW(
        scm, XyreControl::kControllerServiceName, SERVICE_QUERY_STATUS);
    const bool installed = service != nullptr;
    const DWORD error = installed ? ERROR_SUCCESS : GetLastError();
    if (service)
        CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return installed || error != ERROR_SERVICE_DOES_NOT_EXIST;
}

ControlResponse ControlServiceClient::send(const QJsonObject& request, int timeoutMs) {
    QJsonObject versionedRequest = request;
    versionedRequest.insert("version", XyreControl::kProtocolVersion);
    const QByteArray payload =
        QJsonDocument(versionedRequest).toJson(QJsonDocument::Compact);
    if (payload.size() > static_cast<int>(XyreControl::kMaxMessageBytes))
        return errorResponse(QStringLiteral("controller request is too large"));

    QElapsedTimer deadline;
    deadline.start();
    ScopedHandle pipe;
    DWORD lastError = ERROR_FILE_NOT_FOUND;

    while (deadline.elapsed() < timeoutMs) {
        const DWORD remaining = static_cast<DWORD>(
            (std::max)(1, timeoutMs - static_cast<int>(deadline.elapsed())));
        if (!WaitNamedPipeW(XyreControl::kPipeName, remaining)) {
            lastError = GetLastError();
            if (!isTransientPipeError(lastError))
                break;
            QThread::msleep(25);
            continue;
        }

        pipe = ScopedHandle(CreateFileW(
            XyreControl::kPipeName,
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            nullptr));
        if (pipe)
            break;

        lastError = GetLastError();
        if (!isTransientPipeError(lastError))
            break;
        QThread::msleep(25);
    }

    if (!pipe) {
        return errorResponse(
            QStringLiteral("controller pipe is unavailable (win32=%1)").arg(lastError),
            XyreExitCode::ServiceStartFailed,
            lastError);
    }

    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe.get(), &mode, nullptr, nullptr))
        return errorResponse(QStringLiteral("failed to configure controller pipe"));

    ScopedHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event)
        return errorResponse(QStringLiteral("failed to create pipe completion event"));

    QByteArray response(static_cast<int>(XyreControl::kMaxMessageBytes), '\0');
    DWORD bytesRead = 0;
    OVERLAPPED overlapped{};
    overlapped.hEvent = event.get();
    BOOL completed = TransactNamedPipe(
        pipe.get(),
        const_cast<char*>(payload.constData()),
        static_cast<DWORD>(payload.size()),
        response.data(),
        static_cast<DWORD>(response.size()),
        &bytesRead,
        &overlapped);

    if (!completed && GetLastError() == ERROR_IO_PENDING) {
        const DWORD remaining = static_cast<DWORD>(
            (std::max)(1, timeoutMs - static_cast<int>(deadline.elapsed())));
        if (WaitForSingleObject(event.get(), remaining) != WAIT_OBJECT_0) {
            CancelIoEx(pipe.get(), &overlapped);
            WaitForSingleObject(event.get(), INFINITE);
            return errorResponse(
                QStringLiteral("controller pipe request timed out"),
                XyreExitCode::ServiceStartFailed,
                ERROR_TIMEOUT);
        }
        completed = GetOverlappedResult(pipe.get(), &overlapped, &bytesRead, FALSE);
    }

    if (!completed) {
        const DWORD error = GetLastError();
        return errorResponse(
            QStringLiteral("controller pipe request failed (win32=%1)").arg(error),
            XyreExitCode::ServiceStartFailed,
            error);
    }

    response.resize(static_cast<int>(bytesRead));
    return parseResponse(response);
}

ControlResponse ControlServiceClient::installElevated(const QString& serviceExePath) {
    const QFileInfo executableInfo(serviceExePath);
    if (!executableInfo.exists() || !executableInfo.isFile()) {
        return errorResponse(
            QStringLiteral("controller executable was not found: %1").arg(serviceExePath));
    }

    const std::wstring executable = serviceExePath.toStdWString();
    constexpr wchar_t parameters[] = L"install-controller";
    ScopedComApartment comApartment;

    SHELLEXECUTEINFOW executeInfo{};
    executeInfo.cbSize = sizeof(executeInfo);
    executeInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    executeInfo.lpVerb = L"runas";
    executeInfo.lpFile = executable.c_str();
    executeInfo.lpParameters = parameters;
    executeInfo.nShow = SW_HIDE;

    if (!ShellExecuteExW(&executeInfo)) {
        const DWORD error = GetLastError();
        const QString detail = error == ERROR_CANCELLED
            ? QStringLiteral("administrator approval was cancelled")
            : QStringLiteral("failed to request administrator approval (win32=%1)").arg(error);
        return errorResponse(detail, XyreExitCode::AccessDenied, error);
    }

    ScopedHandle process(executeInfo.hProcess);
    if (WaitForSingleObject(process.get(), 30'000) != WAIT_OBJECT_0)
        return errorResponse(QStringLiteral("controller installation timed out"));

    DWORD exitCode = XyreExitCode::UnexpectedError;
    if (!GetExitCodeProcess(process.get(), &exitCode))
        return errorResponse(QStringLiteral("failed to read controller installer result"));
    if (!XyreExitCode::isSoftCode(static_cast<int>(exitCode))) {
        return errorResponse(
            QStringLiteral("controller installation failed with exit code %1").arg(exitCode),
            static_cast<int>(exitCode));
    }

    return { true, static_cast<int>(exitCode), 0, {}, {}, {} };
}

ControlResponse ControlServiceClient::ensureAvailable(const QString& serviceExePath) {
    const QJsonObject ping{
        { QStringLiteral("command"), QStringLiteral("ping") }
    };
    ControlResponse response = send(ping, 250);
    if (response.ok)
        return response;

    if (!isControllerInstalled()) {
        response = installElevated(serviceExePath);
        if (!response.ok)
            return response;
    }
    else {
        response = startInstalledController();
        if (!response.ok) {
            response = installElevated(serviceExePath);
            if (!response.ok)
                return response;
        }
    }

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5'000) {
        response = send(ping, 250);
        if (response.ok)
            return response;
        QThread::msleep(100);
    }

    response = installElevated(serviceExePath);
    if (!response.ok)
        return response;

    timer.restart();
    while (timer.elapsed() < 5'000) {
        response = send(ping, 250);
        if (response.ok)
            return response;
        QThread::msleep(100);
    }
    return errorResponse(
        QStringLiteral("Xyre controller did not respond after installation or upgrade"),
        XyreExitCode::ServiceStartFailed);
}

TrafficStatsResponse ControlServiceClient::queryTraffic(const QString& adapterName, int timeoutMs) {
    const ControlResponse response = send(QJsonObject{
        { QStringLiteral("command"), QStringLiteral("traffic") },
        { QStringLiteral("adapterName"), adapterName }
    }, timeoutMs);
    if (!response.ok)
        return { false, 0, 0, 0, response.detail };

    bool rxOk = false;
    bool txOk = false;
    bool handshakeOk = false;
    const quint64 rx = response.payload.value("rxBytes").toString().toULongLong(&rxOk);
    const quint64 tx = response.payload.value("txBytes").toString().toULongLong(&txOk);
    const qint64 lastHandshake =
        response.payload.value("lastHandshakeMsec").toString().toLongLong(&handshakeOk);
    if (!rxOk || !txOk || !handshakeOk) {
        return {
            false, 0, 0, 0,
            QStringLiteral("controller returned invalid traffic counters")
        };
    }
    return { true, rx, tx, lastHandshake, {} };
}

RingLogResponse ControlServiceClient::queryRingLog(const QString& configPath, int timeoutMs) {
    const ControlResponse response = send(QJsonObject{
        { QStringLiteral("command"), QStringLiteral("ringlog") },
        { QStringLiteral("configPath"), QFileInfo(configPath).absoluteFilePath() }
    }, timeoutMs);
    if (!response.ok)
        return { false, {}, response.detail };

    QStringList lines;
    const QJsonValue linesValue = response.payload.value("lines");
    if (!linesValue.isArray())
        return { false, {}, QStringLiteral("controller returned invalid ring log data") };

    const QJsonArray array = linesValue.toArray();
    lines.reserve(array.size());
    for (const QJsonValue& value : array) {
        if (value.isString())
            lines.append(value.toString());
    }
    return { true, lines, {} };
}
