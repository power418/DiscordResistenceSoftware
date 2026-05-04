#include <rpc/platform/theme.hpp>

#if defined(_WIN32)
#  include <dwmapi.h>

#  include <winrt/Windows.Foundation.h>
#  include <winrt/base.h>
#  include <winrt/Windows.UI.ViewManagement.h>

#  include <memory>
#  include <utility>

namespace {

#  ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#    define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#  endif

[[nodiscard]] bool read_theme_light_setting(const wchar_t* value_name, bool& uses_light) {
  constexpr wchar_t kPersonalizeKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

  DWORD value = 0;
  DWORD value_size = sizeof(value);
  const LSTATUS status = RegGetValueW(
    HKEY_CURRENT_USER,
    kPersonalizeKey,
    value_name,
    RRF_RT_REG_DWORD,
    nullptr,
    &value,
    &value_size);
  if (status != ERROR_SUCCESS) {
    return false;
  }

  uses_light = value != 0;
  return true;
}

void ensure_winrt_apartment() {
  static const bool initialized = [] {
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    return true;
  }();
  (void)initialized;
}

[[nodiscard]] bool is_light_color(const winrt::Windows::UI::Color& color) {
  return (((5 * color.G) + (2 * color.R) + color.B) > (8 * 128));
}

struct ThemeSyncState {
  HWND hwnd = nullptr;
  winrt::Windows::UI::ViewManagement::UISettings settings{};
  winrt::Windows::UI::ViewManagement::UISettings::ColorValuesChanged_revoker revoker{};
};

std::unique_ptr<ThemeSyncState> g_theme_sync;

} // namespace

namespace rpc::platform {

bool is_system_dark_mode() {
  ensure_winrt_apartment();

  bool uses_light = true;
  if (read_theme_light_setting(L"SystemUsesLightTheme", uses_light)) {
    return !uses_light;
  }

  if (read_theme_light_setting(L"AppsUseLightTheme", uses_light)) {
    return !uses_light;
  }

  try {
    winrt::Windows::UI::ViewManagement::UISettings settings;
    const auto background =
      settings.GetColorValue(winrt::Windows::UI::ViewManagement::UIColorType::Background);
    return !is_light_color(background);
  } catch (...) {
    return true;
  }
}

void apply_window_theme(void* window_handle) {
  HWND hwnd = static_cast<HWND>(window_handle);
  if (hwnd == nullptr) {
    return;
  }

  const BOOL use_dark = is_system_dark_mode() ? TRUE : FALSE;
  const DWMWINDOWATTRIBUTE dark_mode_attributes[] = {
    static_cast<DWMWINDOWATTRIBUTE>(DWMWA_USE_IMMERSIVE_DARK_MODE),
    static_cast<DWMWINDOWATTRIBUTE>(19),
  };
  for (const auto attribute : dark_mode_attributes) {
    if (SUCCEEDED(DwmSetWindowAttribute(
          hwnd,
          attribute,
          &use_dark,
          sizeof(use_dark)))) {
      break;
    }
  }

  SetWindowPos(
    hwnd,
    nullptr,
    0,
    0,
    0,
    0,
    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void start_theme_sync(void* window_handle) {
  HWND hwnd = static_cast<HWND>(window_handle);
  if (hwnd == nullptr) {
    return;
  }

  ensure_winrt_apartment();

  g_theme_sync.reset();

  try {
    auto state = std::make_unique<ThemeSyncState>();
    state->hwnd = hwnd;
    state->settings = winrt::Windows::UI::ViewManagement::UISettings{};
    state->revoker = state->settings.ColorValuesChanged(
      winrt::auto_revoke,
      [hwnd](auto const&, auto const&) {
        if (hwnd != nullptr && IsWindow(hwnd)) {
          PostMessageW(hwnd, kThemeSyncMessage, 0, 0);
        }
      });
    g_theme_sync = std::move(state);
  } catch (...) {
    g_theme_sync.reset();
  }

  apply_window_theme(hwnd);
}

void stop_theme_sync() {
  g_theme_sync.reset();
}

} // namespace rpc::platform

#endif
