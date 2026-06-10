#include "ControlServiceClient.h"

#include "ControlProtocol.h"
#include "ExitCodes.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QThread>

#include <Windows.h>
#include <shellapi.h>

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
    return { false, code, win32, detail, {} };
}

ControlResponse parseResponse(const QByteArray& bytes) {
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return errorResponse(QStringLiteral("controller returned an invalid response"));

    const QJsonObject response = document.object();
    return {
        response.value("ok").toBool(),
        response.value("code").toInt(XyreExitCode::UnexpectedError),
        static_cast<DWORD>(response.value("win32").toInteger()),
        response.value("detail").toString(),
        response.value("serviceName").toString()
    };
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
    return { true, XyreExitCode::Ok, 0, {}, {} };
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

    if (!WaitNamedPipeW(XyreControl::kPipeName, static_cast<DWORD>(timeoutMs))) {
        const DWORD error = GetLastError();
        return errorResponse(
            QStringLiteral("controller pipe is unavailable (win32=%1)").arg(error),
            XyreExitCode::ServiceStartFailed,
            error);
    }

    ScopedHandle pipe(CreateFileW(
        XyreControl::kPipeName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        nullptr));
    if (!pipe) {
        const DWORD error = GetLastError();
        return errorResponse(
            QStringLiteral("failed to connect to controller pipe (win32=%1)").arg(error),
            XyreExitCode::ServiceStartFailed,
            error);
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
        if (WaitForSingleObject(event.get(), static_cast<DWORD>(timeoutMs)) != WAIT_OBJECT_0) {
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

    return { true, static_cast<int>(exitCode), 0, {}, {} };
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

    return errorResponse(
        QStringLiteral("Xyre controller is installed but not responding; check Windows Services"),
        XyreExitCode::ServiceStartFailed);
}
