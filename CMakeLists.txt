cmake_minimum_required(VERSION 3.28)

project(software_discord_rpc
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

include(cmake/Dependencies.cmake)

option(SOFTWARE_RPC_ENABLE_LTO "Enable link-time optimization (Release-like builds)" ON)
option(SOFTWARE_RPC_ENABLE_NATIVE_OPT "Enable -march=native (GCC/Clang only)" OFF)

add_library(rpc_core)

target_sources(rpc_core
  PRIVATE
    src/app/tray.cpp
  PUBLIC
    FILE_SET CXX_MODULES FILES
      modules/rpc/rpc.cppm
      modules/rpc/app/shell.cppm
      modules/rpc/core.cppm
      modules/rpc/activity.cppm
      modules/rpc/config.cppm
      modules/rpc/core/orchestrator.cppm
      modules/rpc/detectors/creative_apps.cppm
      modules/rpc/discord/ipc_client.cppm
      modules/rpc/os/active_window.cppm
      modules/rpc/utils/logger.cppm
    FILE_SET HEADERS FILES
      include/rpc/activity.hpp
      include/platform/tray.hpp
)

target_include_directories(rpc_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(rpc_core PUBLIC cxx_std_20)

software_rpc_add_dependencies(rpc_core)

if (WIN32)
  target_link_libraries(rpc_core PRIVATE user32 shell32 gdi32 comctl32)
endif()

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
else()
  target_compile_options(rpc_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(software_discord_rpc
  src/main.cpp
)

target_link_libraries(software_discord_rpc PRIVATE rpc_core)

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
