#pragma once

#include <string_view>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#elif defined(__APPLE__)
#  include <cstddef>
#endif

namespace rpc { struct Config; }

namespace rpc::platform {

#if defined(_WIN32)
void show_settings_dialog(
  HWND owner,
  HINSTANCE instance,
  HICON icon,
  rpc::Config& config);
#elif defined(__APPLE__)
void show_settings_dialog(
  void* owner,
  void* instance,
  void* icon,
  rpc::Config& config);
#endif

} // namespace rpc::platform
