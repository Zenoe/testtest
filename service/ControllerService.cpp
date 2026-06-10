#include "Driver.h"
#include "ControllerService.h"
#include "service.h"
#include "slogger.h"
#include "../common/ControlProtocol.h"
#include "../common/ExitCodes.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>

#include <Windows.h>
#include <sddl.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace {

SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
SERVICE_STATUS g_status{};
HANDLE g_stopEvent = nullptr;

struct TrafficFailureState {
    quint32 count = 0;
    QString detail;
};
QHash<QString, TrafficFailureState> g_trafficFailures;

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

int exitCodeFor(const ServiceResult& result) {
    using Status = ServiceResult::Status;
    switch (result.status) {
        case Status::Ok:             return XyreExitCode::Ok;
        case Status::AlreadyExists:  return XyreExitCode::AlreadyExists;
        case Status::AlreadyRunning: return XyreExitCode::AlreadyRunning;
        case Status::AlreadyStopped: return XyreExitCode::AlreadyStopped;
        case Status::NotFound:       return XyreExitCode::NotFound;
        case Status::AccessDenied:   return XyreExitCode::AccessDenied;
        default:                     return XyreExitCode::UnexpectedError;
    }
}

QByteArray responseFor(const ServiceResult& result, bool ok,
                       const QString& serviceName = {}) {
    QJsonObject response{
        { "ok", ok },
        { "code", exitCodeFor(result) },
        { "win32", static_cast<qint64>(result.win32) },
        { "detail", result.detail },
        { "serviceName", serviceName },
        { "version", XyreControl::kProtocolVersion }
    };
    return QJsonDocument(response).toJson(QJsonDocument::Compact);
}

QByteArray errorResponse(const QString& detail, int code = XyreExitCode::InvalidArguments) {
    return QJsonDocument(QJsonObject{
        { "ok", false },
        { "code", code },
        { "win32", 0 },
        { "detail", detail },
        { "version", XyreControl::kProtocolVersion }
    }).toJson(QJsonDocument::Compact);
}

QString tunnelServiceName(const QString& configPath) {
    return QStringLiteral("XyGuardTunnel$") + QFileInfo(configPath).baseName();
}

bool isValidAdapterName(const QString& adapterName) {
    if (adapterName.isEmpty() || adapterName.size() > 128
        || adapterName != adapterName.trimmed()) {
        return false;
    }
    for (const QChar ch : adapterName) {
        if (ch.isNull() || !ch.isPrint() || ch == '/' || ch == '\\')
            return false;
    }
    return true;
}

bool isManagedTunnelRunning(const QString& adapterName) {
    const QString serviceName = QStringLiteral("XyGuardTunnel$") + adapterName;
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm)
        return false;

    SC_HANDLE service = OpenServiceW(
        scm, reinterpret_cast<LPCWSTR>(serviceName.utf16()), SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_STATUS_PROCESS status{};
    DWORD bytesNeeded = 0;
    const bool running = QueryServiceStatusEx(
        service,
        SC_STATUS_PROCESS_INFO,
        reinterpret_cast<LPBYTE>(&status),
        sizeof(status),
        &bytesNeeded)
        && status.dwCurrentState == SERVICE_RUNNING;
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return running;
}

QByteArray trafficResponse(const QString& adapterName) {
    QElapsedTimer timer;
    timer.start();
    try {
        auto adapter = Tunnel::Driver::Adapter::open(adapterName.toStdWString());
        const Tunnel::Interface config = adapter.getConfiguration();

        quint64 rxBytes = 0;
        quint64 txBytes = 0;
        qint64 lastHandshakeMsec = 0;
        for (const Tunnel::Peer& peer : config.peers) {
            if (std::numeric_limits<quint64>::max() - rxBytes < peer.rxBytes
                || std::numeric_limits<quint64>::max() - txBytes < peer.txBytes) {
                throw std::overflow_error("traffic counter overflow");
            }
            rxBytes += peer.rxBytes;
            txBytes += peer.txBytes;
            lastHandshakeMsec = (std::max)(lastHandshakeMsec, peer.lastHandshakeMsec);
        }

        spdlog::trace(
            "[Controller] traffic | adapter={} peers={} rx={} tx={} elapsedMs={}",
            adapterName.toStdString(), config.peers.size(), rxBytes, txBytes, timer.elapsed());
        const auto failure = g_trafficFailures.take(adapterName);
        if (failure.count > 0) {
            spdlog::info(
                "[Controller] traffic recovered | adapter={} failures={}",
                adapterName.toStdString(), failure.count);
        }
        return QJsonDocument(QJsonObject{
            { "ok", true },
            { "code", XyreExitCode::Ok },
            { "win32", 0 },
            { "detail", QString{} },
            { "adapterName", adapterName },
            { "rxBytes", QString::number(rxBytes) },
            { "txBytes", QString::number(txBytes) },
            { "lastHandshakeMsec", QString::number(lastHandshakeMsec) },
            { "version", XyreControl::kProtocolVersion }
        }).toJson(QJsonDocument::Compact);
    }
    catch (const std::exception& ex) {
        const QString detail = QString::fromUtf8(ex.what());
        TrafficFailureState& failure = g_trafficFailures[adapterName];
        ++failure.count;
        if (failure.count == 1 || failure.count % 30 == 0 || failure.detail != detail) {
            spdlog::warn(
                "[Controller] traffic failed | adapter={} failures={} elapsedMs={} error={}",
                adapterName.toStdString(), failure.count, timer.elapsed(), ex.what());
        }
        failure.detail = detail;
        return errorResponse(detail, XyreExitCode::UnexpectedError);
    }
}

QByteArray dispatch(const QByteArray& bytes) {
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return errorResponse(QStringLiteral("invalid JSON request"));

    const QJsonObject request = document.object();
    if (request.value("version").toInt() != XyreControl::kProtocolVersion) {
        spdlog::warn("[Controller] rejected unsupported protocol version");
        return errorResponse(QStringLiteral("unsupported protocol version"));
    }

    const QString command = request.value("command").toString();
    if (command == QStringLiteral("traffic") || command == QStringLiteral("ping"))
        spdlog::trace("[Controller] request | command={}", command.toStdString());
    else
        spdlog::info("[Controller] request | command={}", command.toStdString());
    if (command == QStringLiteral("ping"))
        return responseFor(ServiceResult::success(), true);

    if (command == QStringLiteral("traffic")) {
        const QString adapterName = request.value("adapterName").toString();
        if (!isValidAdapterName(adapterName)) {
            spdlog::warn("[Controller] rejected invalid traffic adapter name");
            return errorResponse(QStringLiteral("invalid adapter name"));
        }
        if (!isManagedTunnelRunning(adapterName)) {
            spdlog::debug(
                "[Controller] traffic unavailable | adapter={} managed tunnel not running",
                adapterName.toStdString());
            return errorResponse(QStringLiteral("managed tunnel is not running"));
        }
        return trafficResponse(adapterName);
    }

    if (command == QStringLiteral("connect")) {
        const QString configPath = request.value("configPath").toString();
        const QFileInfo config(configPath);
        if (configPath.isEmpty() || !config.isAbsolute() || !config.exists()
            || !config.isFile() || !config.isReadable()) {
            return errorResponse(
                QStringLiteral("config does not exist or is not readable: %1").arg(configPath));
        }

        wchar_t exePath[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0)
            return errorResponse(QStringLiteral("failed to resolve controller executable"));

        const QString serviceName = tunnelServiceName(configPath);
        const QString displayName =
            QStringLiteral("XyGuardTunnel : ") + config.baseName();
        const ServiceResult addResult = Tunnel::Service::add(
            serviceName, displayName, QString::fromWCharArray(exePath), configPath);
        if (!addResult.soft())
            return responseFor(addResult, false, serviceName);

        const ServiceResult startResult = Tunnel::Service::start(serviceName);
        const bool started =
            startResult.status == ServiceResult::Status::Ok
            || startResult.status == ServiceResult::Status::AlreadyRunning;
        if (!started && addResult.ok())
            Tunnel::Service::remove(serviceName);
        return responseFor(startResult, started, serviceName);
    }

    if (command == QStringLiteral("disconnect")) {
        const QString serviceName = request.value("serviceName").toString();
        if (serviceName.isEmpty() || !serviceName.startsWith(QStringLiteral("XyGuardTunnel$")))
            return errorResponse(QStringLiteral("invalid tunnel service name"));

        const ServiceResult stopResult = Tunnel::Service::stop(serviceName);
        if (!stopResult.soft())
            return responseFor(stopResult, false, serviceName);
        const ServiceResult removeResult = Tunnel::Service::remove(serviceName);
        g_trafficFailures.remove(serviceName.mid(QStringLiteral("XyGuardTunnel$").size()));
        return responseFor(removeResult, removeResult.soft(), serviceName);
    }

    return errorResponse(QStringLiteral("unknown command: %1").arg(command));
}

bool finishIo(HANDLE pipe, OVERLAPPED& overlapped, DWORD timeoutMs, DWORD& transferred) {
    HANDLE handles[] = { g_stopEvent, overlapped.hEvent };
    const DWORD wait = WaitForMultipleObjects(2, handles, FALSE, timeoutMs);
    if (wait != WAIT_OBJECT_0 + 1) {
        CancelIoEx(pipe, &overlapped);
        WaitForSingleObject(overlapped.hEvent, INFINITE);
        return false;
    }
    return GetOverlappedResult(pipe, &overlapped, &transferred, FALSE) == TRUE;
}

bool readMessage(HANDLE pipe, QByteArray& output) {
    ScopedHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event)
        return false;

    output.resize(static_cast<int>(XyreControl::kMaxMessageBytes));
    OVERLAPPED overlapped{};
    overlapped.hEvent = event.get();
    DWORD bytesRead = 0;
    BOOL completed = ReadFile(
        pipe, output.data(), static_cast<DWORD>(output.size()), &bytesRead, &overlapped);
    if (!completed && GetLastError() == ERROR_IO_PENDING)
        completed = finishIo(pipe, overlapped, 10'000, bytesRead);
    if (!completed)
        return false;
    output.resize(static_cast<int>(bytesRead));
    return true;
}

bool writeMessage(HANDLE pipe, const QByteArray& response) {
    ScopedHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event)
        return false;

    OVERLAPPED overlapped{};
    overlapped.hEvent = event.get();
    DWORD bytesWritten = 0;
    BOOL completed = WriteFile(
        pipe, response.constData(), static_cast<DWORD>(response.size()),
        &bytesWritten, &overlapped);
    if (!completed && GetLastError() == ERROR_IO_PENDING)
        completed = finishIo(pipe, overlapped, 5'000, bytesWritten);
    return completed && bytesWritten == static_cast<DWORD>(response.size());
}

SECURITY_ATTRIBUTES pipeSecurity(PSECURITY_DESCRIPTOR& descriptor) {
    // SYSTEM/admins have full access; an interactive desktop user may read/write requests.
    constexpr wchar_t sddl[] = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;IU)";
    descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            sddl, SDDL_REVISION_1, &descriptor, nullptr)) {
        return {};
    }
    SECURITY_ATTRIBUTES attributes{
        sizeof(SECURITY_ATTRIBUTES),
        descriptor,
        FALSE
    };
    return attributes;
}

bool configureControllerDacl(SC_HANDLE service) {
    // Interactive users may query/start the controller, but cannot stop, reconfigure, or delete it.
    constexpr wchar_t sddl[] =
        L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;CCLCSWRPLORC;;;IU)";
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            sddl, SDDL_REVISION_1, &descriptor, nullptr)) {
        return false;
    }
    const BOOL ok = SetServiceObjectSecurity(service, DACL_SECURITY_INFORMATION, descriptor);
    LocalFree(descriptor);
    return ok == TRUE;
}

ServiceResult stopControllerForUpgrade(SC_HANDLE service) {
    SERVICE_STATUS_PROCESS status{};
    DWORD bytesNeeded = 0;
    if (!QueryServiceStatusEx(
            service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status),
            sizeof(status), &bytesNeeded)) {
        return ServiceResult::fromWin32(GetLastError(), "upgrade controller/query status");
    }
    if (status.dwCurrentState == SERVICE_STOPPED)
        return ServiceResult::success();

    SERVICE_STATUS controlStatus{};
    if (!ControlService(service, SERVICE_CONTROL_STOP, &controlStatus)
        && GetLastError() != ERROR_SERVICE_NOT_ACTIVE) {
        return ServiceResult::fromWin32(GetLastError(), "upgrade controller/stop");
    }

    const ULONGLONG deadline = GetTickCount64() + 15'000;
    while (GetTickCount64() < deadline) {
        if (!QueryServiceStatusEx(
                service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status),
                sizeof(status), &bytesNeeded)) {
            return ServiceResult::fromWin32(GetLastError(), "upgrade controller/wait status");
        }
        if (status.dwCurrentState == SERVICE_STOPPED)
            return ServiceResult::success();
        Sleep(100);
    }
    return {
        ServiceResult::Status::Failed,
        ERROR_SERVICE_REQUEST_TIMEOUT,
        QStringLiteral("timed out stopping controller for upgrade")
    };
}

void setServiceState(DWORD state, DWORD win32ExitCode = NO_ERROR, DWORD waitHint = 0) {
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwCurrentState = state;
    g_status.dwControlsAccepted =
        state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
    g_status.dwWin32ExitCode = win32ExitCode;
    g_status.dwWaitHint = waitHint;
    SetServiceStatus(g_statusHandle, &g_status);
}

DWORD WINAPI controlHandler(DWORD control, DWORD, LPVOID, LPVOID) {
    if (control == SERVICE_CONTROL_INTERROGATE)
        return NO_ERROR;
    if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN) {
        setServiceState(SERVICE_STOP_PENDING, NO_ERROR, 15'000);
        SetEvent(g_stopEvent);
        return NO_ERROR;
    }
    return ERROR_CALL_NOT_IMPLEMENTED;
}

void runPipeServer() {
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    SECURITY_ATTRIBUTES security = pipeSecurity(descriptor);
    if (!descriptor)
        throw std::runtime_error("failed to create pipe security descriptor");

    while (WaitForSingleObject(g_stopEvent, 0) != WAIT_OBJECT_0) {
        ScopedHandle pipe(CreateNamedPipeW(
            XyreControl::kPipeName,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            PIPE_UNLIMITED_INSTANCES,
            XyreControl::kMaxMessageBytes,
            XyreControl::kMaxMessageBytes,
            0,
            &security));
        if (!pipe) {
            LocalFree(descriptor);
            throw std::runtime_error("CreateNamedPipeW failed");
        }

        ScopedHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!event)
            continue;
        OVERLAPPED overlapped{};
        overlapped.hEvent = event.get();
        DWORD transferred = 0;
        BOOL connected = ConnectNamedPipe(pipe.get(), &overlapped);
        if (!connected) {
            const DWORD error = GetLastError();
            if (error == ERROR_PIPE_CONNECTED)
                connected = TRUE;
            else if (error == ERROR_IO_PENDING)
                connected = finishIo(pipe.get(), overlapped, INFINITE, transferred);
        }
        if (!connected)
            continue;
        if (WaitForSingleObject(g_stopEvent, 0) == WAIT_OBJECT_0)
            break;

        QByteArray request;
        const QByteArray response = readMessage(pipe.get(), request)
            ? dispatch(request)
            : errorResponse(QStringLiteral("invalid or oversized pipe request"));
        writeMessage(pipe.get(), response);
        DisconnectNamedPipe(pipe.get());
    }

    LocalFree(descriptor);
}

void WINAPI serviceMain(DWORD, wchar_t**) {
    g_statusHandle = RegisterServiceCtrlHandlerExW(
        XyreControl::kControllerServiceName, controlHandler, nullptr);
    if (!g_statusHandle)
        return;

    setServiceState(SERVICE_START_PENDING, NO_ERROR, 3'000);
    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) {
        setServiceState(SERVICE_STOPPED, GetLastError());
        return;
    }

    try {
        setServiceState(SERVICE_RUNNING);
        runPipeServer();
        setServiceState(SERVICE_STOPPED);
    }
    catch (const std::exception& ex) {
        spdlog::error("[Controller] {}", ex.what());
        setServiceState(SERVICE_STOPPED, ERROR_GEN_FAILURE);
    }

    CloseHandle(g_stopEvent);
    g_stopEvent = nullptr;
}

} // namespace

int ControllerService::runDispatcher() {
    SERVICE_TABLE_ENTRYW table[] = {
        { const_cast<LPWSTR>(XyreControl::kControllerServiceName), serviceMain },
        { nullptr, nullptr }
    };
    if (!StartServiceCtrlDispatcherW(table))
        return XyreExitCode::ServiceStartFailed;
    return XyreExitCode::Ok;
}

ServiceResult ControllerService::install(const QString& exePath) {
    SC_HANDLE scm = OpenSCManagerW(
        nullptr, nullptr, SC_MANAGER_CREATE_SERVICE | SC_MANAGER_CONNECT);
    if (!scm)
        return ServiceResult::fromWin32(GetLastError(), "install controller/openSCM");

    const QString commandLine =
        QStringLiteral("\"%1\" /controller-service").arg(exePath);
    SC_HANDLE service = CreateServiceW(
        scm,
        XyreControl::kControllerServiceName,
        XyreControl::kControllerDisplayName,
        SERVICE_START | SERVICE_QUERY_STATUS | SERVICE_CHANGE_CONFIG | DELETE | WRITE_DAC,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        reinterpret_cast<LPCWSTR>(commandLine.utf16()),
        nullptr, nullptr, nullptr, nullptr, nullptr);

    bool created = service != nullptr;
    if (!service) {
        const DWORD error = GetLastError();
        if (error != ERROR_SERVICE_EXISTS) {
            CloseServiceHandle(scm);
            return ServiceResult::fromWin32(error, "install controller/CreateService");
        }

        service = OpenServiceW(
            scm,
            XyreControl::kControllerServiceName,
            SERVICE_START | SERVICE_STOP | SERVICE_QUERY_STATUS
                | SERVICE_CHANGE_CONFIG | DELETE | WRITE_DAC);
        if (!service) {
            const DWORD openError = GetLastError();
            CloseServiceHandle(scm);
            return ServiceResult::fromWin32(openError, "install controller/OpenService");
        }

        spdlog::info("[Controller] upgrading installed controller service");
        const ServiceResult stopResult = stopControllerForUpgrade(service);
        if (!stopResult.ok()) {
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            return stopResult;
        }

        if (!ChangeServiceConfigW(
                service,
                SERVICE_NO_CHANGE,
                SERVICE_AUTO_START,
                SERVICE_ERROR_NORMAL,
                reinterpret_cast<LPCWSTR>(commandLine.utf16()),
                nullptr, nullptr, nullptr, nullptr, nullptr,
                XyreControl::kControllerDisplayName)) {
            const DWORD configError = GetLastError();
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            return ServiceResult::fromWin32(configError, "install controller/ChangeServiceConfig");
        }
    }

    if (!configureControllerDacl(service)) {
        const DWORD error = GetLastError();
        if (created)
            DeleteService(service);
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return ServiceResult::fromWin32(error, "install controller/security");
    }

    SERVICE_DESCRIPTIONW description{
        const_cast<LPWSTR>(L"Privileged controller for Xyre WireGuard tunnel services.")
    };
    ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION, &description);

    SERVICE_FAILURE_ACTIONSW failureActions{};
    SC_ACTION actions[] = {
        { SC_ACTION_RESTART, 2'000 },
        { SC_ACTION_RESTART, 5'000 },
        { SC_ACTION_RESTART, 10'000 }
    };
    failureActions.dwResetPeriod = 24 * 60 * 60;
    failureActions.cActions = static_cast<DWORD>(std::size(actions));
    failureActions.lpsaActions = actions;
    ChangeServiceConfig2W(service, SERVICE_CONFIG_FAILURE_ACTIONS, &failureActions);

    ServiceResult result = ServiceResult::success();
    if (!StartServiceW(service, 0, nullptr)) {
        const DWORD error = GetLastError();
        if (error != ERROR_SERVICE_ALREADY_RUNNING) {
            result = ServiceResult::fromWin32(error, "install controller/StartService");
            if (created)
                DeleteService(service);
        }
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return result;
}
