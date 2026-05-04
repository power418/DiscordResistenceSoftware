include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")

function(software_rpc_add_dependencies target_name)
  if(NOT TARGET "${target_name}")
    message(FATAL_ERROR "software_rpc_add_dependencies(): target '${target_name}' not found")
  endif()

  # fmt (header-only) - used by spdlog and general formatting.
  # NOTE: fmt 11.1+ fixes a Clang >= 20 consteval regression that breaks builds.
  CPMAddPackage("gh:fmtlib/fmt#11.1.0")

  # spdlog (header-only) - logging.
  set(SPDLOG_FMT_EXTERNAL_HO ON CACHE BOOL "Use external fmt header-only" FORCE)
  set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
  CPMAddPackage("gh:gabime/spdlog#v1.15.3")

  # nlohmann/json (header-only) - config parsing.
  set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
  CPMAddPackage("gh:nlohmann/json#v3.11.3")

  # lodepng - PNG encoder/decoder (single file, zero dependencies).
  CPMAddPackage(
    NAME lodepng
    GITHUB_REPOSITORY lvandeve/lodepng
    GIT_TAG master
    DOWNLOAD_ONLY YES
  )

  if(lodepng_ADDED AND NOT TARGET lodepng)
    add_library(lodepng STATIC "${lodepng_SOURCE_DIR}/lodepng.cpp")
    target_include_directories(lodepng PUBLIC "${lodepng_SOURCE_DIR}")
    # Suppress warnings in third-party code
    if(MSVC)
      target_compile_options(lodepng PRIVATE /w)
    else()
      target_compile_options(lodepng PRIVATE -w)
    endif()
  endif()

  target_link_libraries("${target_name}"
    PUBLIC
      fmt::fmt-header-only
      spdlog::spdlog_header_only
      nlohmann_json::nlohmann_json
      lodepng
  )
endfunction()
