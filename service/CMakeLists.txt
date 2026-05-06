# Removed the redundant cmake_minimum_required and project() — since this is always included via add_subdirectory(), the root governs those. The C++ standard is also inherited.
# Moved add_definitions(-DUNICODE -D_UNICODE) to the top as it's a global concern for this target.
# Added PRIVATE to target_link_libraries (was missing).
# Removed the entire manual DLL copy block — since both targets land in the same CMAKE_BINARY_DIR and the root already runs windeployqt + copies lib/bin, there's nothing left to do. The old code is preserved commented out for standalone-build scenarios.
# service/CMakeLists.txt
# Note: cmake_minimum_required and project() are intentionally omitted here
# because this file is included via add_subdirectory() from the root.
# The root project governs version, language, and C++ standard.

add_definitions(-DUNICODE -D_UNICODE)

# ── Dependencies ─────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(spdlog CONFIG REQUIRED)

# ── Target: xyreService ──────────────────────────────────────────────────────
add_executable(xyreService
    smain.cpp
    service.h
    service.cpp
    slogger.h
    slogger.cpp
    #tunnel/Driver.h
    #tunnel/Driver.cpp
    # PipeServer.cpp
    # CommandDispatcher.cpp
)

target_include_directories(xyreService PRIVATE
    ${CMAKE_SOURCE_DIR}/common
)

# # ── Admin elevation manifest (Windows only) ──────────────────────────────────
# if(WIN32)
# 	if(MSVC)
# 		 target_link_options(xyreService PRIVATE /MANIFEST:NO)
# 	 endif()

#     # Embed the manifest so Windows UAC prompts for elevation on launch.
#     # The mt.exe approach works with both MSVC and MinGW toolchains via CMake.
#     target_sources(xyreService PRIVATE
#         "${CMAKE_CURRENT_SOURCE_DIR}/xyreService.manifest"
#     )
#     set_source_files_properties(
#         "${CMAKE_CURRENT_SOURCE_DIR}/xyreService.manifest"
#         PROPERTIES VS_TOOL_OVERRIDE "Manifest"   # MSVC: picked up by the linker automatically
#     )

#     # For MinGW / non-MSVC: embed via a resource file that references the manifest
#     if(NOT MSVC)
#         configure_file(
#             "${CMAKE_CURRENT_SOURCE_DIR}/xyreService.manifest"
#             "${CMAKE_CURRENT_BINARY_DIR}/xyreService.manifest"
#             COPYONLY
#         )
#         # Write a minimal .rc that embeds the manifest (resource ID 1, type RT_MANIFEST=24)
#         file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/xyreService_manifest.rc"
#             "#include <windows.h>\n"
#             "1 RT_MANIFEST \"${CMAKE_CURRENT_BINARY_DIR}/xyreService.manifest\"\n"
#         )
#         target_sources(xyreService PRIVATE
#             "${CMAKE_CURRENT_BINARY_DIR}/xyreService_manifest.rc"
#         )
#     endif()
# endif()

target_link_libraries(xyreService PRIVATE
    Qt6::Core
    advapi32
    spdlog::spdlog
)

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
