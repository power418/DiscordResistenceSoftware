module;

#include <string>
#include <filesystem>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

export module rpc.os.autostart;

#if defined(_WIN32)
namespace {
    const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const wchar_t* kValueName = L"software_discord_rpc";

    std::wstring get_executable_path() {
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return std::wstring(buffer);
    }
}
#endif

export namespace rpc::os {

#if defined(_WIN32)
[[nodiscard]] inline bool is_autostart_enabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type;
    DWORD size = 0;
    bool enabled = (RegQueryValueExW(hKey, kValueName, nullptr, &type, nullptr, &size) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    return enabled;
}

inline void set_autostart_enabled(bool enabled) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return;
    }

    if (enabled) {
        std::wstring path = L"\"" + get_executable_path() + L"\"";
        RegSetValueExW(hKey, kValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(path.c_str()), 
                       static_cast<DWORD>((path.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, kValueName);
    }
    RegCloseKey(hKey);
}
#else
[[nodiscard]] inline bool is_autostart_enabled() { return false; }
inline void set_autostart_enabled(bool) {}
#endif

} // namespace rpc::os
