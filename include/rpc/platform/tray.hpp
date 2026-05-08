#pragma once

#include <cstdint>

namespace rpc::platform {

inline constexpr std::uint32_t kTrayCallbackMessage = 0x8001;
inline constexpr std::uint32_t kTrayMenuShow = 2001;
inline constexpr std::uint32_t kTrayMenuRecentActivity = 2002;
inline constexpr std::uint32_t kTrayMenuSettings = 2003;
inline constexpr std::uint32_t kTrayMenuExit = 2004;

struct TrayConfig {
  void* window_handle = nullptr;
  std::uint32_t icon_id = 1;
  void* icon_handle = nullptr;
  const wchar_t* tooltip = L"software_discord_rpc is running";
};

bool init_tray_platform();
bool add_tray_icon(const TrayConfig& config);
bool show_tray_balloon(void* window_handle,
                       std::uint32_t icon_id,
                       const wchar_t* title,
                       const wchar_t* message);
void remove_tray_icon(void* window_handle, std::uint32_t icon_id);
void show_tray_context_menu(void* window_handle);
std::uint32_t last_tray_error();

} // namespace rpc::platform
