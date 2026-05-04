#pragma once

#if defined(_WIN32)
#  include <rpc/config/win32.h>
#  include <windows.h>
#endif

namespace rpc::platform {

#if defined(_WIN32)
inline constexpr UINT kThemeSyncMessage = WM_APP + 73;
[[nodiscard]] bool is_system_dark_mode();
void apply_window_theme(void* window_handle);
void start_theme_sync(void* window_handle);
void stop_theme_sync();
#endif

} // namespace rpc::platform
