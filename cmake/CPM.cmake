# Lightweight bootstrapper for CPM.cmake.
# This file downloads a pinned CPM.cmake version at configure-time (once per build/cache).
#
# Users can speed things up by sharing a cache folder across projects:
#   -DCPM_SOURCE_CACHE=<path>

if(DEFINED CPM_CMAKE_INCLUDED)
  return()
endif()
set(CPM_CMAKE_INCLUDED TRUE)

# Keep this pinned for reproducible builds. Can be overridden by cache.
set(CPM_DOWNLOAD_VERSION "0.42.0" CACHE STRING "CPM.cmake version to bootstrap")

if(NOT DEFINED CPM_DOWNLOAD_LOCATION)
  if(DEFINED CPM_SOURCE_CACHE AND CPM_SOURCE_CACHE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
  else()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
  endif()
endif()

get_filename_component(CPM_DOWNLOAD_LOCATION "${CPM_DOWNLOAD_LOCATION}" ABSOLUTE)
get_filename_component(_cpm_download_dir "${CPM_DOWNLOAD_LOCATION}" DIRECTORY)
file(MAKE_DIRECTORY "${_cpm_download_dir}")

if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
  message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION} -> ${CPM_DOWNLOAD_LOCATION}")
  file(
    DOWNLOAD
      "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
      "${CPM_DOWNLOAD_LOCATION}"
    TLS_VERIFY ON
    SHOW_PROGRESS
    STATUS _cpm_status
  )
  list(GET _cpm_status 0 _cpm_status_code)
  list(GET _cpm_status 1 _cpm_status_string)
  if(NOT _cpm_status_code EQUAL 0)
    message(FATAL_ERROR "Failed to download CPM.cmake: ${_cpm_status_code} ${_cpm_status_string}")
  endif()
endif()

include("${CPM_DOWNLOAD_LOCATION}")

