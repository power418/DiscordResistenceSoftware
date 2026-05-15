#pragma once

#include <string_view>

// Win32 desktop baseline for the app shell.
// Keep this aligned with the Win32 shell and any future WinRT/WinUI adapter.
#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif

#  ifndef RPC_WINDOWS_TARGET_VERSION
#    define RPC_WINDOWS_TARGET_VERSION 0x0A00
#  endif

#  ifndef WINVER
#    define WINVER RPC_WINDOWS_TARGET_VERSION
#  endif

#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT RPC_WINDOWS_TARGET_VERSION
#  endif

#  define RPC_WINDOWS_COMMON_CONTROLS_MANIFEST \
    "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' " \
    "version='6.0.0.0' processorArchitecture='*' " \
    "publicKeyToken='6595b64144ccf1df' language='*'\""

#  pragma comment(linker, RPC_WINDOWS_COMMON_CONTROLS_MANIFEST)
#endif

namespace rpc::config::win32 {

extern const std::wstring_view app_name;
extern const std::wstring_view window_class_name;
extern const std::wstring_view tray_tooltip;
extern const std::wstring_view splash_message;
extern const std::wstring_view recent_activity_title;
extern const std::string_view common_controls_manifest_dependency;

} // namespace rpc::config::win32
