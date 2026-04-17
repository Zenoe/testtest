cmake_minimum_required(VERSION 3.24)
project(Revelation VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core Widgets Network)

find_package(spdlog CONFIG REQUIRED)

qt_add_executable(Revelation
    src/main.cpp
    src/app/Application.cpp
    src/ui/MainWindow.cpp
    src/ui/navigation/NavigationWidget.cpp
    src/ui/navigation/NavItem.cpp
    src/ui/pages/ServerConfigWidget.cpp
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
    src/ui/components/SettingsDialog.cpp
    src/ui/components/AvatarWidget.cpp
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
