#pragma once

/**
 * @file export.hpp
 * @brief Macro definitions for DLL symbol visibility and linkage.
 * 
 * ===========================================================================
 * PROFESSOR ANTIGRAVITY'S EXPORT GUIDE
 * ===========================================================================
 * This file handles the cross-platform complexities of symbol visibility.
 * On Windows, we use __declspec(dllexport/dllimport).
 * On Linux/GCC, we use __attribute__((visibility("default"))).
 * 
 * Usage:
 *   RPC_CORE_API void my_function();
 * ===========================================================================
 */

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef RPC_CORE_EXPORTS
    #define RPC_CORE_API __declspec(dllexport)
  #else
    #define RPC_CORE_API __declspec(dllimport)
  #endif
#else
  #if __GNUC__ >= 4
    #define RPC_CORE_API __attribute__((visibility("default")))
  #else
    #define RPC_CORE_API
  #endif
#endif
