#include <rpc/res/rpc_res.hpp>

extern "C" {
  extern const unsigned char rpc_logo_png[];
  extern const unsigned int rpc_logo_png_size;
  
  extern const unsigned char rpc_logo_transparent_png[];
  extern const unsigned int rpc_logo_transparent_png_size;
}

extern "C" {

RPC_RES_API const unsigned char* rpc_res_get_logo(size_t* out_size) {
    if (out_size) *out_size = rpc_logo_png_size;
    return rpc_logo_png;
}

RPC_RES_API const unsigned char* rpc_res_get_logo_transparent(size_t* out_size) {
    if (out_size) *out_size = rpc_logo_transparent_png_size;
    return rpc_logo_transparent_png;
}

}
