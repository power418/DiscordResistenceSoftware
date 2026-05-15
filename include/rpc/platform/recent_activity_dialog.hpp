#pragma once

#include <string_view>

#if defined(_WIN32)
#  include <rpc/platform/theme.hpp>
#endif

namespace rpc::platform {

#if defined(_WIN32)
void show_recent_activity_dialog(
  HWND owner,
  HINSTANCE instance,
  HICON icon,
  std::wstring_view title,
  std::wstring_view summary);
#elif defined(__APPLE__)
void show_recent_activity_dialog(
  void* owner,
  void* instance,
  void* icon,
  std::string_view title,
  std::string_view summary);
#endif

} // namespace rpc::platform
