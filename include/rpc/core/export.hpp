#pragma once

#if defined(_WIN32)
  #ifdef RPC_CORE_EXPORTS
    #define RPC_CORE_API __declspec(dllexport)
  #else
    #define RPC_CORE_API __declspec(dllimport)
  #endif
#else
  #define RPC_CORE_API __attribute__((visibility("default")))
#endif
