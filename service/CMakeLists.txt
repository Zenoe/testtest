# Removed the redundant cmake_minimum_required and project() — since this is always included via add_subdirectory(), the root governs those. The C++ standard is also inherited.
# Moved add_definitions(-DUNICODE -D_UNICODE) to the top as it's a global concern for this target.
# Added PRIVATE to target_link_libraries (was missing).
# Removed the entire manual DLL copy block — since both targets land in the same CMAKE_BINARY_DIR and the root already runs windeployqt + copies lib/bin, there's nothing left to do. The old code is preserved commented out for standalone-build scenarios.
# service/CMakeLists.txt
# Note: cmake_minimum_required and project() are intentionally omitted here
# because this file is included via add_subdirectory() from the root.
# The root project governs version, language, and C++ standard.

# ── Dependencies ─────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(spdlog CONFIG REQUIRED)

# ── Target: xyreService ──────────────────────────────────────────────────────
add_executable(xyreService
    smain.cpp
    service.h
    service.cpp
    ControllerService.h
    ControllerService.cpp
    slogger.h
    slogger.cpp
    tunnel/Driver.h
    tunnel/Driver.cpp
    tunnel/WireGuardTypes.h
    # PipeServer.cpp
    # CommandDispatcher.cpp
)

target_include_directories(xyreService PRIVATE
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/tunnel
)

target_compile_definitions(xyreService PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(xyreService PRIVATE
    Qt6::Core
    advapi32
    spdlog::spdlog
    ws2_32
)
if(MSVC)
    target_link_options(xyreService PRIVATE "/MANIFESTUAC:level='asInvoker'")
endif()

# Output all configurations next to Revelation (i.e. the root binary dir)
set_target_properties(xyreService PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY                "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_BINARY_DIR}"
)

# ── Post-build: copy DLLs (Windows only) ────────────────────────────────────
# lib/bin DLLs are already copied by the root target (Revelation).
# Here we only need to ensure Qt6Core is present; windeployqt in the root
# covers the full Qt runtime so no manual copy is needed either.
# If xyreService is ever built standalone (outside add_subdirectory), uncomment:
#
# if(WIN32)
#     get_target_property(_qt6_core_loc Qt6::Core IMPORTED_LOCATION)
#     get_filename_component(_qt6_bin_dir "${_qt6_core_loc}" DIRECTORY)
#     find_file(_qt6core_dll
#         NAMES Qt6Cored.dll Qt6Core.dll
#         PATHS "${_qt6_bin_dir}"
#         NO_DEFAULT_PATH
#     )
#     add_custom_command(TARGET xyreService POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -E copy_if_different
#             "${_qt6core_dll}"
#             $<TARGET_FILE_DIR:xyreService>
#         COMMENT "Copying Qt6Core DLL to output directory"
#     )
# endif()
