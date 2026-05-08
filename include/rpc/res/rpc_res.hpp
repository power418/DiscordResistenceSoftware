#pragma once

#if defined(_WIN32)
  #ifdef RPC_RES_EXPORTS
    #define RPC_RES_API __declspec(dllexport)
  #else
    #define RPC_RES_API __declspec(dllimport)
  #endif
#else
  #define RPC_RES_API __attribute__((visibility("default")))
#endif

#include <cstddef>

extern "C" {

RPC_RES_API const unsigned char* rpc_res_get_logo(size_t* out_size);
RPC_RES_API const unsigned char* rpc_res_get_logo_transparent(size_t* out_size);

}
