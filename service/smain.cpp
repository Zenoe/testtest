#include "service.h"
#include "ControllerService.h"
#include "slogger.h"

#include <shlobj.h>
#include <iostream>
#include <filesystem>

#include "../common/ExitCodes.h"
#include "../common/commonstringutil.h"
using EC = XyreExitCode::Code;

// Maps a ServiceResult status to the appropriate exit code
static EC toExitCode(const ServiceResult& r) {
    switch (r.status) {
        case ServiceResult::Status::Ok:             return EC::Ok;
        case ServiceResult::Status::AlreadyExists:  return EC::AlreadyExists;
        case ServiceResult::Status::AlreadyRunning: return EC::AlreadyRunning;
        case ServiceResult::Status::AlreadyStopped: return EC::AlreadyStopped;
        case ServiceResult::Status::NotFound:       return EC::NotFound;
        case ServiceResult::Status::AccessDenied:   return EC::AccessDenied;
        default:                                    return EC::UnexpectedError;
    }
}

static std::string getServiceLogPath() {
	PWSTR path_tmp = nullptr;
	const HRESULT hr = SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &path_tmp);
	if (FAILED(hr) || !path_tmp)
		return "";

	const std::filesystem::path logDir =
		std::filesystem::path(path_tmp) / L"XY" / L"Revelation" / L"logs";
	CoTaskMemFree(path_tmp);
	std::error_code error;
	std::filesystem::create_directories(logDir, error);
	return (logDir / L"service.log").u8string();
}
static void logResult(const ServiceResult& r, const std::string& ctx) {
    if (r.ok())
        spdlog::info("wmain | {} | OK", ctx);
    else if (r.soft())
        spdlog::warn("wmain | {} | soft: {} (win32={})",
                     ctx, r.detail.toStdString(), r.win32);
    else {
        spdlog::error("wmain | {} | failed: {} (win32={})",
                      ctx, r.detail.toStdString(), r.win32);
        // Helper failures are returned through stderr to the app.
        std::wcerr << L"Failed at " << QString::fromStdString(ctx).toStdWString()
                   << L": " << r.detail.toStdWString()
                   << L" (win32=" << r.win32 << L")\n";
    }
}

int wmain(int argc, wchar_t* argv[]) {
	if (argc < 2) {
		std::wcerr << L"Usage: xyreService.exe <command> [configPath|serviceName]\n";
		return EC::InvalidArguments;
	}

	setup_logging(getServiceLogPath());
	const std::wstring cmd = argv[1];
	if (cmd == L"/controller-service")
		return ControllerService::runDispatcher();

	if (cmd == L"install-controller") {
		wchar_t path[MAX_PATH]{};
		if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0)
			return EC::UnexpectedError;
		const ServiceResult result =
			ControllerService::install(QString::fromWCharArray(path));
		logResult(result, "install-controller");
		return toExitCode(result);
	}

	if (argc == 3) {
		const std::wstring secondArg = argv[2];
		spdlog::info("wmain | command={}", to_utf8(cmd));
		if (cmd == L"/service") {
			spdlog::info("wmain | calling WireGuardTunnelService with config path {}", to_utf8(argv[2]));
			try {
				Tunnel::Service::run(QString::fromStdWString(argv[2]));
				return EC::Ok;
			}
			catch (const std::exception& ex) {
				spdlog::error("wmain | WireGuardTunnelService failed: {}", ex.what());
				return EC::ServiceStartFailed;
			}
		}

		ServiceResult result;
		spdlog::info("wmain | cmd={} arg={}", to_utf8(cmd), to_utf8(secondArg));

		if (cmd == L"add") {
			wchar_t path[MAX_PATH];
			GetModuleFileNameW(NULL, path, MAX_PATH);
			QString exePath = QString::fromWCharArray(path);
			// secondArg is configPath
			std::wstring basename = common::getBasename(secondArg);
			QString serviceName = "XyGuardTunnel$" + QString::fromStdWString(basename);
			QString disName = "XyGuardTunnel : " + QString::fromStdWString(basename);

			result = Tunnel::Service::add(serviceName, disName, exePath, QString::fromStdWString(secondArg));
		}
		else {
			QString serviceName = QString::fromStdWString(argv[2]);
			if (cmd == L"start") {
				spdlog::info("start service: {}", to_utf8(argv[2]));
			result = Tunnel::Service::start(serviceName);
		}
		else if (cmd == L"uninstall") {
			spdlog::info("uninstall service: {}", to_utf8(argv[2]));
			result = Tunnel::Service::remove(serviceName);
		}
		else if (cmd == L"stop") {
			spdlog::info("uninstall service: {}", to_utf8(argv[2]));
			result = Tunnel::Service::stop(serviceName);
		}
		else {
			spdlog::warn("unknown cmd: {}", to_utf8(cmd));
			return EC::UnknownCommand;
		}
	}

	logResult(result, to_utf8(cmd));
	return toExitCode(result);
	}

	std::wcerr << L"Invalid arguments for command: " << cmd << L"\n";
	return EC::InvalidArguments;
}
