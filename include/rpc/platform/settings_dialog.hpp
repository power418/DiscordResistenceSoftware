#pragma once

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

import rpc.config;

namespace rpc::platform {

#if defined(_WIN32)
void show_settings_dialog(
  HWND owner,
  HINSTANCE instance,
  HICON icon,
  rpc::Config& config);
#endif

} // namespace rpc::platform
