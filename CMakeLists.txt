cmake_minimum_required(VERSION 3.24)
project(Revelation VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Core Widgets Network)

find_package(spdlog CONFIG REQUIRED)

set(COMMON_SOURCES
    src/secure/ISecureStorage.h
    src/secure/SecureStorageFactory.cpp
)

if (WIN32)
    set(PLATFORM_SOURCES
        src/secure/WinCredentialStorage.cpp
        src/secure/WinCredentialStorage.h
    )
elseif (APPLE)
    set(PLATFORM_SOURCES
        #MacKeychainStorage.mm   # Objective-C++
    )
endif()

qt_add_executable(Revelation
    src/main.cpp
    src/app/Application.cpp
    src/ui/MainWindow.cpp
    src/ui/navigation/NavigationWidget.cpp
    src/ui/navigation/NavItem.cpp
    src/ui/pages/ServerConfigWidget.cpp
    src/ui/pages/LoginWidget.h
    src/ui/pages/LoginWidget.cpp
    src/ui/pages/AppGridWidget.cpp
    src/ui/components/AppItemWidget.cpp
    #src/backend/ConfigManager.cpp
    src/backend/NetworkManager.cpp
    src/backend/SessionManager.cpp
    src/backend/AppLauncher.cpp
    src/utils/logger.h
    src/utils/logger.cpp
    src/utils/ConfigManager.h
    src/utils/ConfigManager.cpp
    src/utils/stringutil.h
    src/ui/components/SettingsDialog.cpp
    src/ui/components/AvatarWidget.cpp
    src/ui/components/ClickableLabel.h
    src/ui/components/ClickableLabel.cpp
    src/models/UserSession.h
    ${COMMON_SOURCES}
    ${PLATFORM_SOURCES}
    app.qrc
)

target_link_libraries(Revelation PRIVATE
    Qt6::Core Qt6::Widgets Qt6::Network
        spdlog::spdlog

)

target_include_directories(Revelation PRIVATE 
    src
    lib/include
)
#target_include_directories(Revelation PRIVATE src)
#target_include_directories(Revelation PRIVATE
#    BEFORE
#    "${CMAKE_CURRENT_SOURCE_DIR}/lib/include"
#)

set_target_properties(Revelation PROPERTIES
    WIN32_EXECUTABLE TRUE)          # No console window on Windows
