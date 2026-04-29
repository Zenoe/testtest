#include "service.h"
#include "slogger.h"
#include <fstream>
#include <chrono>
#include <ctime>
#include "../common/ExitCodes.h"

using EC = XyreExitCode::Code;

static std::wstring getBasename(const std::wstring& path) {
	size_t lastSlash = path.find_last_of(L"/\\");
	size_t lastDot = path.find_last_of(L'.');

	std::wstring basename = (lastSlash == std::wstring::npos) ? path : path.substr(lastSlash + 1);

	// Remove extension if present
	if (lastDot != std::wstring::npos && lastDot > lastSlash) {
		basename = basename.substr(0, basename.find_last_of(L'.'));
	}

	return basename;
}

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

static void logResult(const ServiceResult& r, const std::string& ctx) {
    if (r.ok())
        spdlog::info("wmain | {} | OK", ctx);
    else if (r.soft())
        spdlog::warn("wmain | {} | soft: {} (win32={})",
                     ctx, r.detail.toStdString(), r.win32);
    else
        spdlog::error("wmain | {} | failed: {} (win32={})",
                      ctx, r.detail.toStdString(), r.win32);
}

int wmain(int argc, wchar_t* argv[]) {
	if (argc == 3) {
		setup_logging("re_scm.log");
		const std::wstring cmd = argv[1];
    const std::wstring secondArg = argv[2];
		spdlog::info("wmain | command={}", to_utf8(cmd));
		if (cmd == L"/service") {
			spdlog::info("wmain | calling WireGuardTunnelService with config path {}" , to_utf8(argv[2]));
			Tunnel::Service::run(QString::fromStdWString(argv[2]));
            return EC::Ok;
    }

    ServiceResult result;
	setup_logging();
	spdlog::info("wmain | cmd={} arg={}", to_utf8(cmd), to_utf8(secondArg));

	if (cmd == L"add") {
		wchar_t path[MAX_PATH];
		GetModuleFileNameW(NULL, path, MAX_PATH);
		QString exePath = QString::fromWCharArray(path);
		// secondArg is configPath
		std::wstring basename = getBasename(secondArg);
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

	setup_logging();
	spdlog::info("wmain | argc={}", argc);
	wchar_t path[MAX_PATH];
	GetModuleFileNameW(NULL, path, MAX_PATH);
	QString serviceName = "WireGuardTunnel$client1";
	QString disName = "WireGuardTunnel : client1";
	QString configPath = "C:/Users/2004l/Downloads/client1.conf";
	QString exePath = QString::fromWCharArray(path);
	if (argc == 2) {
		std::wstring cmd = argv[1];
        spdlog::info("wmain | command={}", to_utf8(cmd));
        if (cmd == L"add")   Tunnel::Service::add(serviceName,disName, exePath, configPath);
        else if (cmd == L"start")     Tunnel::Service::start(serviceName);
		else if (cmd == L"uninstall") Tunnel::Service::remove(serviceName);
		else if (cmd == L"stop")      Tunnel::Service::stop(serviceName);
    }
    else {
		spdlog::warn("wmain | expected 2 arguments (command + config path), got {}", argc - 1);
    }
    spdlog::info("wmain | exiting");
    return 0;
}
