#pragma once

#if defined(_WIN32)
#  include <rpc/config/win32.h>
#  include <windows.h>
#endif

namespace rpc::platform {

#if defined(_WIN32)
[[nodiscard]] HICON load_app_icon();
#endif

} // namespace rpc::platform
