cmake_minimum_required(VERSION 3.28)

project(software_discord_rpc
  VERSION 1.0.1
  LANGUAGES CXX
)

if (APPLE)
  enable_language(OBJCXX)
  set(CMAKE_OBJCXX_STANDARD 20)
  set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
  set(CMAKE_OBJCXX_EXTENSIONS OFF)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

include(cmake/Dependencies.cmake)

option(SOFTWARE_RPC_ENABLE_LTO "Enable link-time optimization (Release-like builds)" ON)
option(SOFTWARE_RPC_ENABLE_NATIVE_OPT "Enable -march=native (GCC/Clang only)" OFF)

if (MSVC)
  add_compile_options(/bigobj /Zc:preprocessor /utf-8 /W4)
endif()

add_library(rpc_res SHARED
  src/res/rpc_res.cpp
  src/res/rpc_res_data.cpp
)

target_include_directories(rpc_res
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(rpc_res PRIVATE RPC_RES_EXPORTS)

add_library(rpc_core SHARED)

target_compile_definitions(rpc_core PRIVATE RPC_CORE_EXPORTS)

target_sources(rpc_core
  PRIVATE
    src/app/tray.cpp
    src/app/theme.cpp
    src/app/recent_activity_dialog.cpp
    src/app/settings_dialog.cpp
    src/config/fonts.cpp
    src/config/win32.cpp
    src/config/url.cpp
    src/config/dotenv.cpp
    src/stb_image.cpp
    modules/rpc/core/recent_activity.cpp
    modules/rpc/os/icon_cache.cpp
    modules/rpc/core/orchestrator.cpp
    modules/rpc/discord/ipc_client.cpp
    modules/rpc/net/imgur_uploader.cpp
    modules/rpc/utils/logger.cpp
    modules/rpc/app/shell.cppm
    modules/rpc/utils/console.cppm
)

set_source_files_properties(
  modules/rpc/app/shell.cppm
  modules/rpc/utils/console.cppm
  PROPERTIES LANGUAGE CXX
)

target_sources(rpc_core
  PUBLIC
    FILE_SET HEADERS FILES
      include/rpc/activity.hpp
      include/rpc/app/shell.hpp
      include/rpc/config/fonts.h
      include/rpc/config/win32.h
      include/rpc/config/url.h
      include/rpc/rpc.hpp
      include/rpc/platform/icon.hpp
      include/rpc/platform/macos_bridge.h
      include/rpc/platform/recent_activity_dialog.hpp
      include/rpc/platform/settings_dialog.hpp
      include/rpc/platform/theme.hpp
      include/rpc/platform/tray.hpp
)

if (WIN32)
  target_sources(rpc_core PRIVATE src/app/icon.cpp)
endif()

if (APPLE)
  target_sources(rpc_core PRIVATE src/app/macos.mm)
endif()

target_include_directories(rpc_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

target_compile_features(rpc_core PUBLIC cxx_std_20)

software_rpc_add_dependencies(rpc_core)

if (WIN32)
  target_link_libraries(rpc_core PRIVATE user32 shell32 gdi32 comctl32 ole32 windowscodecs winhttp crypt32 dwmapi windowsapp)
endif()

if (APPLE)
  target_compile_options(rpc_core PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
  target_link_libraries(rpc_core PRIVATE "-framework Cocoa" "-framework ApplicationServices" proc)
endif()

target_link_libraries(rpc_core PRIVATE rpc_res)

if (UNIX AND NOT APPLE)
  find_package(X11 QUIET)
  if (X11_FOUND)
    target_compile_definitions(rpc_core PRIVATE SOFTWARE_RPC_HAS_X11=1)
    target_include_directories(rpc_core PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(rpc_core PRIVATE ${X11_LIBRARIES})
  endif()

  find_package(PkgConfig QUIET)
  if (PkgConfig_FOUND)
    pkg_check_modules(GTK3 QUIET gtk+-3.0)
    pkg_check_modules(AYATANA_APPINDICATOR QUIET ayatana-appindicator3-0.1)
    pkg_check_modules(APPINDICATOR QUIET appindicator3-0.1)

    if (GTK3_FOUND AND AYATANA_APPINDICATOR_FOUND)
      target_compile_definitions(rpc_core PRIVATE
        SOFTWARE_RPC_HAS_APPINDICATOR=1
        SOFTWARE_RPC_HAS_AYATANA_APPINDICATOR=1
      )
      target_include_directories(rpc_core PRIVATE
        ${GTK3_INCLUDE_DIRS}
        ${AYATANA_APPINDICATOR_INCLUDE_DIRS}
      )
      target_link_directories(rpc_core PRIVATE
        ${GTK3_LIBRARY_DIRS}
        ${AYATANA_APPINDICATOR_LIBRARY_DIRS}
      )
      target_compile_options(rpc_core PRIVATE
        ${GTK3_CFLAGS_OTHER}
        ${AYATANA_APPINDICATOR_CFLAGS_OTHER}
      )
      target_link_libraries(rpc_core PRIVATE
        ${GTK3_LIBRARIES}
        ${AYATANA_APPINDICATOR_LIBRARIES}
      )
    elseif (GTK3_FOUND AND APPINDICATOR_FOUND)
      target_compile_definitions(rpc_core PRIVATE SOFTWARE_RPC_HAS_APPINDICATOR=1)
      target_include_directories(rpc_core PRIVATE
        ${GTK3_INCLUDE_DIRS}
        ${APPINDICATOR_INCLUDE_DIRS}
      )
      target_link_directories(rpc_core PRIVATE
        ${GTK3_LIBRARY_DIRS}
        ${APPINDICATOR_LIBRARY_DIRS}
      )
      target_compile_options(rpc_core PRIVATE
        ${GTK3_CFLAGS_OTHER}
        ${APPINDICATOR_CFLAGS_OTHER}
      )
      target_link_libraries(rpc_core PRIVATE
        ${GTK3_LIBRARIES}
        ${APPINDICATOR_LIBRARIES}
      )
    endif()
  endif()
endif()

# Warnings (reasonable defaults; keep noise low)
if (MSVC)
  target_compile_options(rpc_core PRIVATE /W4 /permissive-)
  set_source_files_properties(src/stb_image.cpp PROPERTIES COMPILE_OPTIONS /w)
else()
  target_compile_options(rpc_core PRIVATE -Wall -Wextra -Wpedantic)
  set_source_files_properties(src/stb_image.cpp PROPERTIES COMPILE_OPTIONS -w)
endif()

if (APPLE)
  set(MACOS_ICON_FILES res/AppIcon.icns)
  set_source_files_properties(${MACOS_ICON_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

add_executable(software_discord_rpc
  src/main.cpp
  src/res/app.rc
  ${MACOS_ICON_FILES}
)

if (WIN32)
  set_target_properties(software_discord_rpc PROPERTIES
    WIN32_EXECUTABLE "$<NOT:$<CONFIG:Debug>>"
  )
endif()

if (APPLE)
  set_target_properties(software_discord_rpc PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.ahmadzanisy.discord-resistence"
    MACOSX_BUNDLE_BUNDLE_NAME "Discord Resistence"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
    # Search for dylibs in Frameworks folder
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@executable_path/../Frameworks"
  )
  
  set_target_properties(rpc_core PROPERTIES INSTALL_NAME_DIR "@rpath")
  set_target_properties(rpc_res PROPERTIES INSTALL_NAME_DIR "@rpath")

  # --- Auto-bundle dylibs during build ---
  add_custom_command(TARGET software_discord_rpc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:software_discord_rpc>/Frameworks"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:rpc_core>" "$<TARGET_BUNDLE_CONTENT_DIR:software_discord_rpc>/Frameworks/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:rpc_res>" "$<TARGET_BUNDLE_CONTENT_DIR:software_discord_rpc>/Frameworks/"
    COMMENT "Bundling dylibs into the .app bundle..."
    VERBATIM
  )
endif()

target_link_libraries(software_discord_rpc PRIVATE rpc_core)

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_link_libraries(software_discord_rpc PRIVATE "$<$<CONFIG:Debug>:msvcprtd>$<$<NOT:$<CONFIG:Debug>>:msvcprt>")
endif()

# Resources are now embedded in rpc_res library

if (SOFTWARE_RPC_ENABLE_LTO)
  set_property(TARGET rpc_core PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  set_property(TARGET rpc_core PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
  set_property(TARGET rpc_core PROPERTY INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
  set_property(TARGET software_discord_rpc PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  set_property(TARGET software_discord_rpc PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
  set_property(TARGET software_discord_rpc PROPERTY INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
endif()

if (SOFTWARE_RPC_ENABLE_NATIVE_OPT AND NOT MSVC)
  target_compile_options(rpc_core PRIVATE -march=native)
  target_compile_options(software_discord_rpc PRIVATE -march=native)
endif()

# --- Installation & Bundling ---

if (APPLE)
  install(TARGETS software_discord_rpc
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
  )
  install(TARGETS rpc_core rpc_res
    LIBRARY DESTINATION software_discord_rpc.app/Contents/Frameworks
  )
else()
  install(TARGETS software_discord_rpc RUNTIME DESTINATION bin)
  install(TARGETS rpc_core rpc_res LIBRARY DESTINATION lib)
endif()

# --- Packaging ---

set(CPACK_PACKAGE_NAME "DiscordResistence")
set(CPACK_PACKAGE_VENDOR "Ahmad Zanisy")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Discord Resistance Software - C++20")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if (APPLE)
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_DMG_VOLUME_NAME "DiscordResistence")
  set(CPACK_SYSTEM_NAME "macOS")
  
  # Auto-generate DMG after build if requested
  add_custom_target(package_dmg
    COMMAND ${CMAKE_CPACK_COMMAND} -G DragNDrop
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS software_discord_rpc
    COMMENT "Generating DMG package..."
  )
endif()

include(CPack)
