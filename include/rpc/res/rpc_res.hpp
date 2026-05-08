/* rpc_res.hpp - v1.0.0 - Embedded Assets for Discord RPC - public domain

   ===========================================================================
   PENJELASAN ARSITEKTUR RESOURCE
   ===========================================================================
   Mahasiswa, perhatikan! File ini sekarang mengikuti pola "Single-File Header".
   Anda hanya perlu menyertakan file ini di project Anda.

   PENGGUNAAN:
   1. Di SATU file C++ (misalnya rpc_res.cpp), lakukan ini:
         #define RPC_RES_IMPLEMENTATION
         #include "rpc/res/rpc_res.hpp"
   
   2. Di file lain yang ingin menggunakan resource, cukup include seperti biasa:
         #include "rpc/res/rpc_res.hpp"

   FILOSOFI:
   - Zero Latency: Aset dibaca langsung dari RAM (.rodata segment).
   - Standalone: Tidak butuh file external .png di runtime.
   - Type Safe: Menggunakan pointer dan size_t untuk menjamin integritas.

   ===========================================================================
*/

#ifndef RPC_RES_INCLUDE_H
#define RPC_RES_INCLUDE_H

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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mengambil pointer ke data Master Logo (PNG).
 * @param out_size [out] Pointer untuk menerima ukuran data dalam byte.
 * @return const unsigned char* Alamat memori statis ke awal data PNG.
 */
RPC_RES_API const unsigned char* rpc_res_get_logo(size_t* out_size);

/**
 * @brief Mengambil pointer ke data Logo Transparan (PNG).
 * @param out_size [out] Pointer untuk menerima ukuran data dalam byte.
 * @return const unsigned char* Alamat memori statis ke awal data PNG.
 */
RPC_RES_API const unsigned char* rpc_res_get_logo_transparent(size_t* out_size);

#ifdef __cplusplus
}
#endif

#endif // RPC_RES_INCLUDE_H

#ifdef RPC_RES_IMPLEMENTATION

/* 
   IMPLEMENTATION SECTION
   Note: The actual raw data is still stored in rpc_res_data.cpp 
   to prevent IDE performance issues with 15MB of hex text.
*/

extern "C" {
  extern const unsigned char rpc_logo_png[];
  extern const unsigned int rpc_logo_png_size;
  
  extern const unsigned char rpc_logo_transparent_png[];
  extern const unsigned int rpc_logo_transparent_png_size;

  RPC_RES_API const unsigned char* rpc_res_get_logo(size_t* out_size) {
      if (out_size) *out_size = (size_t)rpc_logo_png_size;
      return rpc_logo_png;
  }

  RPC_RES_API const unsigned char* rpc_res_get_logo_transparent(size_t* out_size) {
      if (out_size) *out_size = (size_t)rpc_logo_transparent_png_size;
      return rpc_logo_transparent_png;
  }
}

#endif // RPC_RES_IMPLEMENTATION
