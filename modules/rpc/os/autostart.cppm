#pragma once

#include <string>
#include <filesystem>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

#if defined(_WIN32)
namespace rpc::os::detail {
    inline constexpr const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    inline constexpr const wchar_t* kValueName = L"software_discord_rpc";

    [[nodiscard]] inline std::wstring get_executable_path() {
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return std::wstring(buffer);
    }
}
#endif

namespace rpc::os {

#if defined(_WIN32)
[[nodiscard]] inline bool is_autostart_enabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, detail::kRunKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type;
    DWORD size = 0;
    bool enabled = (RegQueryValueExW(hKey, detail::kValueName, nullptr, &type, nullptr, &size) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    return enabled;
}

inline void set_autostart_enabled(bool enabled) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, detail::kRunKey, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return;
    }

    if (enabled) {
        std::wstring path = L"\"" + detail::get_executable_path() + L"\"";
        RegSetValueExW(hKey, detail::kValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(path.c_str()), 
                       static_cast<DWORD>((path.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, detail::kValueName);
    }
    RegCloseKey(hKey);
}
#else
[[nodiscard]] inline bool is_autostart_enabled() { return false; }
inline void set_autostart_enabled(bool) {}
#endif

} // namespace rpc::os
