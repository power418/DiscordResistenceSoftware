#include <rpc/config/win32.h>
#include <rpc/config/app.h>
#include <string_view>
namespace rpc::config::win32 {

extern const std::wstring_view app_name = rpc::config::kDefaultAppNameWide;
extern const std::wstring_view window_class_name = L"SoftwareDiscordRpcAppWindow";
extern const std::wstring_view tray_tooltip = rpc::config::kDefaultTrayTooltipWide;
extern const std::wstring_view splash_message = L"Discord RPC monitor active. This window will hide to tray.";
extern const std::wstring_view recent_activity_title = L"Recent activity";
#if defined(_WIN32)
extern const std::string_view common_controls_manifest_dependency = RPC_WINDOWS_COMMON_CONTROLS_MANIFEST;
#else
extern const std::string_view common_controls_manifest_dependency = "";
#endif

} // namespace rpc::config::win32
