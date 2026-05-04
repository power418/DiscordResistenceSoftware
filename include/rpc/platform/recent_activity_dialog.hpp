#pragma once

#if defined(_WIN32)
#  include <rpc/platform/theme.hpp>

#  include <string_view>
#endif

namespace rpc::platform {

#if defined(_WIN32)
void show_recent_activity_dialog(
  HWND owner,
  HINSTANCE instance,
  HICON icon,
  std::wstring_view title,
  std::wstring_view summary);
#endif

} // namespace rpc::platform
