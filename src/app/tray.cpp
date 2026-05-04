#include <rpc/platform/tray.hpp>
#include <rpc/config/win32.h>

import rpc.config;

#if defined(_WIN32)
#  include <windows.h>
#  include <commctrl.h>
#  include <shellapi.h>

#  include <cstddef>

namespace {

constexpr WORD kDefaultAppIconResource = 32512;
DWORD g_last_tray_error = ERROR_SUCCESS;
namespace win = rpc::config::win32;

[[nodiscard]] HWND to_hwnd(void* handle) {
  return static_cast<HWND>(handle);
}

template <std::size_t N>
void copy_wide(wchar_t (&destination)[N], const wchar_t* source) {
  if (source == nullptr) {
    destination[0] = L'\0';
    return;
  }

  std::size_t index = 0;
  for (; index + 1 < N && source[index] != L'\0'; ++index) {
    destination[index] = source[index];
  }
  destination[index] = L'\0';
}

[[nodiscard]] NOTIFYICONDATAW make_icon_data(HWND hwnd, std::uint32_t icon_id) {
  NOTIFYICONDATAW data{};
  data.cbSize = sizeof(data);
  data.hWnd = hwnd;
  data.uID = icon_id;
  return data;
}

void set_last_tray_error() {
  const DWORD error = GetLastError();
  g_last_tray_error = error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error;
}

} // namespace

namespace rpc::platform {

bool init_tray_platform() {
  INITCOMMONCONTROLSEX controls{};
  controls.dwSize = sizeof(controls);
  controls.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;

  if (InitCommonControlsEx(&controls) == FALSE) {
    const DWORD error = GetLastError();
    InitCommonControls();
    g_last_tray_error = error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error;
    return true;
  }

  g_last_tray_error = ERROR_SUCCESS;
  return true;
}

bool add_tray_icon(const TrayConfig& config) {
  HWND hwnd = to_hwnd(config.window_handle);
  if (hwnd == nullptr) {
    g_last_tray_error = ERROR_INVALID_WINDOW_HANDLE;
    return false;
  }

  NOTIFYICONDATAW data = make_icon_data(hwnd, config.icon_id);
  data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  data.uCallbackMessage = kTrayCallbackMessage;
  data.hIcon = config.icon_handle
    ? static_cast<HICON>(config.icon_handle)
    : LoadIconW(nullptr, MAKEINTRESOURCEW(kDefaultAppIconResource));
  copy_wide(data.szTip, config.tooltip);

  if (Shell_NotifyIconW(NIM_ADD, &data) == FALSE) {
    set_last_tray_error();
    return false;
  }

  data.uVersion = NOTIFYICON_VERSION_4;
  if (Shell_NotifyIconW(NIM_SETVERSION, &data) == FALSE) {
    set_last_tray_error();
    remove_tray_icon(hwnd, config.icon_id);
    return false;
  }

  g_last_tray_error = ERROR_SUCCESS;
  return true;
}

bool show_tray_balloon(void* window_handle,
                       std::uint32_t icon_id,
                       const wchar_t* title,
                       const wchar_t* message) {
  HWND hwnd = to_hwnd(window_handle);
  if (hwnd == nullptr) {
    g_last_tray_error = ERROR_INVALID_WINDOW_HANDLE;
    return false;
  }

  NOTIFYICONDATAW data = make_icon_data(hwnd, icon_id);
  data.uFlags = NIF_INFO;
  data.dwInfoFlags = NIIF_INFO;
  const wchar_t* balloon_title =
    (title != nullptr && title[0] != L'\0') ? title : win::app_name.data();
  copy_wide(data.szInfoTitle, balloon_title);
  copy_wide(data.szInfo, message);

  if (Shell_NotifyIconW(NIM_MODIFY, &data) == FALSE) {
    set_last_tray_error();
    return false;
  }

  g_last_tray_error = ERROR_SUCCESS;
  return true;
}

void remove_tray_icon(void* window_handle, std::uint32_t icon_id) {
  HWND hwnd = to_hwnd(window_handle);
  if (hwnd == nullptr) {
    return;
  }

  NOTIFYICONDATAW data = make_icon_data(hwnd, icon_id);
  Shell_NotifyIconW(NIM_DELETE, &data);
}

void show_tray_context_menu(void* window_handle) {
  HWND hwnd = to_hwnd(window_handle);
  if (hwnd == nullptr) {
    g_last_tray_error = ERROR_INVALID_WINDOW_HANDLE;
    return;
  }

  POINT cursor{};
  GetCursorPos(&cursor);

  HMENU menu = CreatePopupMenu();
  if (menu == nullptr) {
    set_last_tray_error();
    return;
  }

  AppendMenuW(menu, MF_STRING, kTrayMenuShow, L"Show");
  AppendMenuW(menu, MF_STRING, kTrayMenuRecentActivity, L"Recent activity");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kTrayMenuExit, L"Exit");

  SetForegroundWindow(hwnd);
  const UINT command = TrackPopupMenu(
    menu,
    TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD,
    cursor.x,
    cursor.y,
    0,
    hwnd,
    nullptr);

  DestroyMenu(menu);

  if (command != 0) {
    PostMessageW(hwnd, WM_COMMAND, command, 0);
  }
}

std::uint32_t last_tray_error() {
  return g_last_tray_error;
}

} // namespace rpc::platform

#else

namespace rpc::platform {

bool init_tray_platform() { return false; }
bool add_tray_icon(const TrayConfig&) { return false; }
bool show_tray_balloon(void*, std::uint32_t, const wchar_t*, const wchar_t*) { return false; }
void remove_tray_icon(void*, std::uint32_t) {}
void show_tray_context_menu(void*) {}
std::uint32_t last_tray_error() { return 0; }

} // namespace rpc::platform

#endif
