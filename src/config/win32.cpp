#include <rpc/config/win32.h>

#include <string_view>

namespace rpc::config::win32 {

extern const std::wstring_view app_name = L"software_discord_rpc";
extern const std::wstring_view window_class_name = L"SoftwareDiscordRpcAppWindow";
extern const std::wstring_view tray_tooltip = L"RPC";
extern const std::wstring_view splash_message = L"Discord RPC monitor active. This window will hide to tray.";
extern const std::wstring_view recent_activity_title = L"Recent activity";
extern const std::string_view common_controls_manifest_dependency = RPC_WINDOWS_COMMON_CONTROLS_MANIFEST;

} // namespace rpc::config::win32
