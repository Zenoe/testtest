# Added CMAKE_CXX_STANDARD_REQUIRED ON and EXTENSIONS OFF — best practice to prevent silent fallback to a different standard.
# Replaced set(PLATFORM_SOURCES ...) + separate if blocks with a single if/elseif using list(APPEND COMMON_SOURCES ...) — cleaner, no separate variable needed.
# Sorted sources into logical groups (ui, backend, utils, models).
# Added a single lib/bin copy with copy_directory — one command covers all DLLs in that folder, no need to enumerate them.
# Used windeployqt to handle the full Qt runtime, which is more robust than manually finding individual DLLs.
# Prefixed internal variables with _ (e.g. _qt6_bin_dir) to avoid polluting the cache.
cmake_minimum_required(VERSION 3.24)
project(Revelation VERSION 1.0 LANGUAGES CXX)

# ── C++ standard ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Qt automation ────────────────────────────────────────────────────────────
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Build-type definitions ───────────────────────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()

# ── Dependencies ─────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
find_package(spdlog CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# ── Platform-specific sources ────────────────────────────────────────────────
set(COMMON_SOURCES
    src/secure/ISecureStorage.h
    src/secure/SecureStorageFactory.cpp
)

if(WIN32)
    list(APPEND COMMON_SOURCES
        src/secure/WinCredentialStorage.cpp
        src/secure/WinCredentialStorage.h
    )
elseif(APPLE)
    # list(APPEND COMMON_SOURCES src/secure/MacKeychainStorage.mm)
endif()

# ── Target: Revelation ───────────────────────────────────────────────────────
qt_add_executable(Revelation
    src/main.cpp
    src/app/Application.cpp
    src/ui/MainWindow.cpp
    src/ui/MainWindow.test.cpp
    src/ui/navigation/NavigationWidget.cpp
    src/ui/navigation/NavItem.cpp
    src/ui/pages/ServerConfigWidget.cpp
    src/ui/pages/LoginWidget.h
    src/ui/pages/LoginWidget.cpp
    src/ui/pages/AppGridWidget.cpp
    src/ui/components/AppItemWidget.cpp
    src/ui/components/SettingsDialog.cpp
    src/ui/components/AvatarWidget.cpp
    src/ui/components/ClickableLabel.h
    src/ui/components/ClickableLabel.cpp
    src/backend/NetworkManager.cpp
    src/backend/SessionManager.cpp
    src/backend/AppLauncher.cpp
    src/backend/ControlServiceClient.cpp
    src/backend/ControlServiceClient.h
    src/backend/VpnManager.cpp
    src/backend/SpaManager.cpp
    src/utils/logger.h
    src/utils/logger.cpp
    src/utils/ConfigManager.h
    src/utils/ConfigManager.cpp
    src/utils/RouteManager.h
    src/utils/RouteManager.cpp
    src/utils/RouteMonitor.h
    src/utils/RouteMonitor.cpp
    
    src/utils/stringutil.h
    src/utils/RouteConflictChecker.cpp
    src/models/UserSession.h
    src/models/VpnConf.cpp
    src/vpn/StatusPanel.h
    src/vpn/StatusPanel.cpp
    src/vpn/RingLogDialog.h
    src/vpn/RingLogDialog.cpp


    src/test/test.cpp
    ${COMMON_SOURCES}
    app.qrc
)

target_include_directories(Revelation PRIVATE
    src
    common
    lib/include
)

target_link_libraries(Revelation PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    spdlog::spdlog
    OpenSSL::SSL
    OpenSSL::Crypto
    advapi32
    ole32
    shell32
)

set_target_properties(Revelation PROPERTIES
    WIN32_EXECUTABLE TRUE   # No console window on Windows
)
if(MSVC)
    target_link_options(Revelation PRIVATE "/MANIFESTUAC:level='asInvoker'")
endif()

# ── Post-build: copy DLLs (Windows only) ────────────────────────────────────
if(WIN32)
    # Copy everything from lib/bin next to the executable
    #add_custom_command(TARGET Revelation POST_BUILD
    #    COMMAND ${CMAKE_COMMAND} -E copy_directory
    #        "${CMAKE_SOURCE_DIR}/lib/bin"
    #        $<TARGET_FILE_DIR:Revelation>
    #    COMMENT "Copying lib/bin DLLs to output directory"
    #)

	file(GLOB DLL_FILES "${CMAKE_SOURCE_DIR}/lib/bin/*.dll")

    foreach(dll ${DLL_FILES})
        add_custom_command(TARGET Revelation POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${dll}"
                $<TARGET_FILE_DIR:Revelation>
            COMMENT "Copying ${dll} if different"
        )
    endforeach()

    # Deploy Qt runtime DLLs via windeployqt
    get_target_property(_qt6_core_loc Qt6::Core IMPORTED_LOCATION)
    get_filename_component(_qt6_bin_dir "${_qt6_core_loc}" DIRECTORY)

    add_custom_command(TARGET Revelation POST_BUILD
        COMMAND "${_qt6_bin_dir}/windeployqt"
			--no-opengl-sw
            --no-translations
            $<TARGET_FILE:Revelation>
        COMMENT "Running windeployqt for Revelation"
    )
endif()

# ── Subdirectory ─────────────────────────────────────────────────────────────
add_subdirectory(service)
