module;

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <thread>

#include <rpc/platform/tray.hpp>
#include <rpc/platform/recent_activity_dialog.hpp>
#include <rpc/platform/theme.hpp>
#include <rpc/platform/settings_dialog.hpp>
#include <rpc/config/win32.h>
#include <rpc/core/export.hpp>

#if defined(_WIN32)
#  include <windows.h>
#  include <rpc/platform/icon.hpp>
#elif defined(__linux__)
#  include <csignal>
#  if defined(SOFTWARE_RPC_HAS_APPINDICATOR)
#    include <gtk/gtk.h>
#    if defined(SOFTWARE_RPC_HAS_AYATANA_APPINDICATOR)
#      include <libayatana-appindicator/app-indicator.h>
#    else
#      include <libappindicator/app-indicator.h>
#    endif
#  endif
#endif

export module rpc.app.shell;

import rpc.core;
import rpc.core.orchestrator;
import rpc.config;
import rpc.utils.logger;
import rpc.os.autostart;

export namespace rpc::app {

struct AppShellOptions {
  std::chrono::milliseconds splash_duration = std::chrono::milliseconds(2500);
  std::chrono::milliseconds poll_interval = std::chrono::milliseconds(1000);
  bool show_splash_on_start = true;
  bool enable_tray = true;
};

RPC_CORE_API int run(AppShellOptions options = {});

} // namespace rpc::app

namespace rpc::app {

#if defined(_WIN32)
namespace {

constexpr UINT_PTR kSplashTimer = 1001;
constexpr UINT_PTR kPollTimer = 1002;
constexpr UINT kTrayIconId = 1;
constexpr WORD kArrowCursorResource = 32512;
constexpr WORD kDefaultAppIconResource = 32512;
constexpr UINT kTraySelectMessage = 0x0400;
constexpr UINT kTrayKeySelectMessage = 0x0401;
namespace win = rpc::config::win32;

[[nodiscard]] std::wstring utf8_to_wide(std::string_view text) {
  if (text.empty()) {
    return {};
  }

  const int required_size = MultiByteToWideChar(
    CP_UTF8,
    0,
    text.data(),
    static_cast<int>(text.size()),
    nullptr,
    0);
  if (required_size <= 0) {
    return {};
  }

  std::wstring wide(static_cast<std::size_t>(required_size), L'\0');
  MultiByteToWideChar(
    CP_UTF8,
    0,
    text.data(),
    static_cast<int>(text.size()),
    wide.data(),
    required_size);
  return wide;
}

[[nodiscard]] UINT timer_ms(std::chrono::milliseconds duration) {
  constexpr auto max_timer = static_cast<std::int64_t>((std::numeric_limits<UINT>::max)());
  const auto count = std::clamp<std::int64_t>(duration.count(), 1, max_timer);
  return static_cast<UINT>(count);
}

class WindowsAppShell {
public:
  explicit WindowsAppShell(AppShellOptions options)
      : options_(options),
        instance_(GetModuleHandleW(nullptr)),
        taskbar_created_message_(RegisterWindowMessageW(L"TaskbarCreated")),
        orchestrator_(options.poll_interval),
        config_(rpc::load_config_or_default(rpc::settings_path())) {}

  ~WindowsAppShell() {
    rpc::platform::stop_theme_sync();
    if (app_icon_owned_ && app_icon_) {
      DestroyIcon(app_icon_);
    }
  }

  int run() {
    rpc::log::init();
    ensure_app_icon();

    if (!rpc::platform::init_tray_platform()) {
      rpc::log::warn("Common controls init failed: {}", rpc::platform::last_tray_error());
    }

    if (!register_window_class()) {
      rpc::log::error("Failed to register app window class");
      return 1;
    }

    if (!create_window()) {
      rpc::log::error("Failed to create app window");
      return 1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
  }

private:
  AppShellOptions options_;
  HINSTANCE instance_{}; 
  HWND hwnd_{}; 
  UINT taskbar_created_message_{};
  bool tray_added_ = false;
  HICON app_icon_ = nullptr;
  HICON taskbar_icon_ = nullptr;
  bool app_icon_owned_ = false;
  rpc::core::Orchestrator orchestrator_;
  rpc::Config config_;

  void ensure_app_icon() {
    if (app_icon_) {
      return;
    }

    app_icon_ = rpc::platform::load_app_icon();
    app_icon_owned_ = (app_icon_ != nullptr);
    if (!app_icon_) {
      app_icon_ = LoadIconW(nullptr, MAKEINTRESOURCEW(kDefaultAppIconResource));
      rpc::log::warn("App icon asset not found; using default application icon");
    }

    // Load taskbar icon from executable resource (ID 1).
    // This icon represents the app in the Taskbar and Start Menu.
    taskbar_icon_ = LoadIconW(instance_, MAKEINTRESOURCEW(1));
    if (!taskbar_icon_) {
      taskbar_icon_ = app_icon_;
    }
  }

  bool register_window_class() const {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &WindowsAppShell::window_proc;
    window_class.hInstance = instance_;
    window_class.hIcon = taskbar_icon_ ? taskbar_icon_ : app_icon_;
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(kArrowCursorResource));
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = win::window_class_name.data();
    window_class.hIconSm = app_icon_;

    return RegisterClassExW(&window_class) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }

  bool create_window() {
    constexpr int width = 460;
    constexpr int height = 180;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    hwnd_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_APPWINDOW,
      win::window_class_name.data(),
      win::app_name.data(),
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
      x,
      y,
      width,
      height,
      nullptr,
      nullptr,
      instance_,
      this);

    if (!hwnd_) {
      return false;
    }

    rpc::platform::start_theme_sync(hwnd_);
    apply_window_icon();
    add_tray_icon();

    if (options_.show_splash_on_start) {
      show_splash();
    }
    return true;
  }

  void show_splash() const {
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd_);
    UpdateWindow(hwnd_);
    SetTimer(hwnd_, kSplashTimer, timer_ms(options_.splash_duration), nullptr);
  }

  void show_recent_activity() const {
    const std::string summary = orchestrator_.recent_activity_summary(10);
    const std::string text = summary.empty()
      ? "No recent activity yet. Open a supported app to build history."
      : summary;
    const std::wstring wide_text = utf8_to_wide(text);

    rpc::platform::show_recent_activity_dialog(
      hwnd_,
      instance_,
      app_icon_,
      win::recent_activity_title,
      wide_text);
  }
  
  void show_settings() {
    rpc::platform::show_settings_dialog(
      hwnd_,
      instance_,
      app_icon_,
      config_);
  }

  void hide_splash() const {
    KillTimer(hwnd_, kSplashTimer);
    ShowWindow(hwnd_, SW_HIDE);
  }

  void add_tray_icon() {
    if (!options_.enable_tray || tray_added_) {
      return;
    }

    const rpc::platform::TrayConfig config{
      .window_handle = hwnd_,
      .icon_id = kTrayIconId,
      .icon_handle = app_icon_,
      .tooltip = win::tray_tooltip.data(),
    };

    if (!rpc::platform::add_tray_icon(config)) {
      rpc::log::warn("Tray icon add failed: {}", rpc::platform::last_tray_error());
      return;
    }

    tray_added_ = true;
    rpc::log::info("Tray icon added");
    show_balloon();
  }

  void remove_tray_icon() {
    if (tray_added_) {
      rpc::platform::remove_tray_icon(hwnd_, kTrayIconId);
      tray_added_ = false;
    }
  }

  void show_balloon() {
    if (!rpc::platform::show_tray_balloon(
          hwnd_,
          kTrayIconId,
          win::app_name.data(),
          L"Running in tray. Double-click to show.")) {
      rpc::log::warn("Tray balloon failed: {}", rpc::platform::last_tray_error());
    }
  }

  void show_context_menu() {
    rpc::platform::show_tray_context_menu(hwnd_);
  }

  void apply_window_icon() const {
    if (!hwnd_) {
      return;
    }

    // ICON_BIG is used for the taskbar (Alt+Tab, etc).
    if (taskbar_icon_) {
      SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(taskbar_icon_));
    }

    // ICON_SMALL is used for the window title bar.
    if (app_icon_) {
      SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(app_icon_));
    }
  }

  void sync_window_theme() const {
    rpc::platform::apply_window_theme(hwnd_);
    InvalidateRect(hwnd_, nullptr, TRUE);
  }

  void paint() const {
    PAINTSTRUCT paint_struct{};
    HDC hdc = BeginPaint(hwnd_, &paint_struct);

    RECT rect{};
    GetClientRect(hwnd_, &rect);

    const bool dark_mode = rpc::platform::is_system_dark_mode();
    const COLORREF background_color = dark_mode ? RGB(32, 34, 43) : RGB(246, 247, 250);
    const COLORREF title_color = dark_mode ? RGB(245, 245, 247) : RGB(22, 24, 29);
    const COLORREF body_color = dark_mode ? RGB(190, 198, 215) : RGB(79, 86, 104);

    HBRUSH background = CreateSolidBrush(background_color);
    FillRect(hdc, &rect, background);
    DeleteObject(background);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, title_color);

    HFONT title_font = CreateFontW(
      24, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH,
      L"Segoe UI");
    HFONT body_font = CreateFontW(
      16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH,
      L"Segoe UI");

    RECT title_rect{24, 28, rect.right - 24, 62};
    HGDIOBJ previous_font = SelectObject(hdc, title_font);
    DrawTextW(hdc, win::app_name.data(), -1, &title_rect, DT_LEFT | DT_SINGLELINE);

    SetTextColor(hdc, body_color);
    RECT body_rect{24, 76, rect.right - 24, rect.bottom - 24};
    SelectObject(hdc, body_font);
    DrawTextW(hdc, win::splash_message.data(),
              -1, &body_rect, DT_LEFT | DT_WORDBREAK);

    SelectObject(hdc, previous_font);
    DeleteObject(title_font);
    DeleteObject(body_font);

    EndPaint(hwnd_, &paint_struct);
  }

  LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    if (taskbar_created_message_ != 0 && message == taskbar_created_message_) {
      tray_added_ = false;
      add_tray_icon();
      return 0;
    }

    switch (message) {
      case WM_CREATE:
        SetTimer(hwnd_, kPollTimer, timer_ms(options_.poll_interval), nullptr);
        return 0;

      case WM_TIMER:
        if (wparam == kSplashTimer) {
          hide_splash();
          return 0;
        }
        if (wparam == kPollTimer) {
          orchestrator_.poll_once();
          return 0;
        }
        break;

      case WM_PAINT:
        paint();
        return 0;

      case WM_SETTINGCHANGE:
      case WM_THEMECHANGED:
      case rpc::platform::kThemeSyncMessage:
        sync_window_theme();
        return 0;

      case WM_CTLCOLORSTATIC: {
        const bool dark_mode = rpc::platform::is_system_dark_mode();
        const COLORREF background_color = dark_mode ? RGB(32, 34, 43) : RGB(246, 247, 250);
        const COLORREF text_color = dark_mode ? RGB(190, 198, 215) : RGB(79, 86, 104);
        
        HDC hdc_static = reinterpret_cast<HDC>(wparam);
        SetTextColor(hdc_static, text_color);
        SetBkColor(hdc_static, background_color);
        
        static HBRUSH background_brush = nullptr;
        if (background_brush) DeleteObject(background_brush);
        background_brush = CreateSolidBrush(background_color);
        return reinterpret_cast<LRESULT>(background_brush);
      }

      case WM_CLOSE:
        hide_splash();
        return 0;

      case WM_DESTROY:
        rpc::platform::stop_theme_sync();
        remove_tray_icon();
        PostQuitMessage(0);
        return 0;

      case WM_COMMAND:
        if (LOWORD(wparam) == rpc::platform::kTrayMenuShow) {
          show_splash();
          return 0;
        }
        if (LOWORD(wparam) == rpc::platform::kTrayMenuRecentActivity) {
          show_recent_activity();
          return 0;
        }
        if (LOWORD(wparam) == rpc::platform::kTrayMenuSettings) {
          show_settings();
          return 0;
        }
        if (LOWORD(wparam) == rpc::platform::kTrayMenuExit) {
          DestroyWindow(hwnd_);
          return 0;
        }
        break;

      case rpc::platform::kTrayCallbackMessage:
        if (LOWORD(lparam) == WM_LBUTTONDBLCLK ||
            LOWORD(lparam) == WM_LBUTTONUP ||
            LOWORD(lparam) == kTraySelectMessage ||
            LOWORD(lparam) == kTrayKeySelectMessage) {
          show_splash();
          return 0;
        }
        if (LOWORD(lparam) == WM_RBUTTONUP || LOWORD(lparam) == WM_CONTEXTMENU) {
          show_context_menu();
          return 0;
        }
        break;
    }

    return DefWindowProcW(hwnd_, message, wparam, lparam);
  }

  static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<WindowsAppShell*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
      self = static_cast<WindowsAppShell*>(create_struct->lpCreateParams);
      self->hwnd_ = hwnd;
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (self) {
      return self->handle_message(message, wparam, lparam);
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
  }
};

} // namespace

int run(AppShellOptions options) {
  WindowsAppShell shell(options);
  return shell.run();
}

#elif defined(__linux__)

#if defined(SOFTWARE_RPC_HAS_APPINDICATOR)
namespace {

class LinuxIndicatorShell {
public:
  explicit LinuxIndicatorShell(AppShellOptions options)
      : options_(options),
        orchestrator_(options.poll_interval) {}

  int run() {
    rpc::log::init();

    int argc = 0;
    char** argv = nullptr;
    gtk_init(&argc, &argv);

    create_window();
    create_indicator();

    if (options_.show_splash_on_start) {
      show_window();
      g_timeout_add(static_cast<guint>(options_.splash_duration.count()), hide_window_cb, this);
    }

    g_timeout_add(static_cast<guint>(options_.poll_interval.count()), poll_cb, this);
    gtk_main();
    return 0;
  }

private:
  AppShellOptions options_;
  GtkWidget* window_ = nullptr;
  AppIndicator* indicator_ = nullptr;
  rpc::core::Orchestrator orchestrator_;

  void create_window() {
    window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_), "software_discord_rpc");
    gtk_window_set_default_size(GTK_WINDOW(window_), 420, 160);
    gtk_window_set_position(GTK_WINDOW(window_), GTK_WIN_POS_CENTER);

    GtkWidget* label = gtk_label_new("Discord RPC monitor active. This window will hide to tray.");
    gtk_container_add(GTK_CONTAINER(window_), label);

    g_signal_connect(window_, "delete-event", G_CALLBACK(delete_event_cb), this);
  }

  void create_indicator() {
    if (!options_.enable_tray) {
      return;
    }

    indicator_ = app_indicator_new(
      "software-discord-rpc",
      "applications-system",
      APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_status(indicator_, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_title(indicator_, "software_discord_rpc");

    GtkWidget* menu = gtk_menu_new();
    GtkWidget* show_item = gtk_menu_item_new_with_label("Show");
    GtkWidget* recent_item = gtk_menu_item_new_with_label("Recent activity");
    GtkWidget* exit_item = gtk_menu_item_new_with_label("Exit");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), show_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), recent_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), exit_item);

    g_signal_connect(show_item, "activate", G_CALLBACK(show_cb), this);
    g_signal_connect(recent_item, "activate", G_CALLBACK(recent_activity_cb), this);
    g_signal_connect(exit_item, "activate", G_CALLBACK(exit_cb), this);
    gtk_widget_show_all(menu);

    app_indicator_set_menu(indicator_, GTK_MENU(menu));
  }

  void show_window() {
    gtk_widget_show_all(window_);
    gtk_window_present(GTK_WINDOW(window_));
  }

  void show_recent_activity() {
    const std::string summary = orchestrator_.recent_activity_summary(10);
    const std::string text = summary.empty()
      ? "No recent activity yet. Open a supported app to build history."
      : summary;

    GtkWidget* dialog = gtk_message_dialog_new(
      GTK_WINDOW(window_),
      GTK_DIALOG_MODAL,
      GTK_MESSAGE_INFO,
      GTK_BUTTONS_OK,
      "%s",
      text.c_str());
    gtk_window_set_title(GTK_WINDOW(dialog), "Recent activity");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  }

  void hide_window() { gtk_widget_hide(window_); }

  static gboolean poll_cb(gpointer data) {
    auto* self = static_cast<LinuxIndicatorShell*>(data);
    self->orchestrator_.poll_once();
    return G_SOURCE_CONTINUE;
  }

  static gboolean hide_window_cb(gpointer data) {
    static_cast<LinuxIndicatorShell*>(data)->hide_window();
    return G_SOURCE_REMOVE;
  }

  static gboolean delete_event_cb(GtkWidget*, GdkEvent*, gpointer data) {
    static_cast<LinuxIndicatorShell*>(data)->hide_window();
    return TRUE;
  }

  static void show_cb(GtkMenuItem*, gpointer data) {
    static_cast<LinuxIndicatorShell*>(data)->show_window();
  }

  static void recent_activity_cb(GtkMenuItem*, gpointer data) {
    static_cast<LinuxIndicatorShell*>(data)->show_recent_activity();
  }

  static void exit_cb(GtkMenuItem*, gpointer) { gtk_main_quit(); }
};

} // namespace

int run(AppShellOptions options) {
  LinuxIndicatorShell shell(options);
  return shell.run();
}

#else
namespace {

std::atomic_bool keep_running = true;

void handle_signal(int) { keep_running = false; }

void show_linux_fallback_notice() {
  std::system("command -v notify-send >/dev/null 2>&1 && "
              "notify-send 'software_discord_rpc' 'Running in background mode' >/dev/null 2>&1");
  std::system("command -v zenity >/dev/null 2>&1 && "
              "zenity --info --timeout=3 --title='software_discord_rpc' "
              "--text='Discord RPC monitor active. Install AppIndicator for tray.' >/dev/null 2>&1 &");
}

} // namespace

int run(AppShellOptions options) {
  rpc::log::init();
  rpc::log::warn("Linux tray dependencies not found; running service fallback");
  show_linux_fallback_notice();

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  rpc::core::Orchestrator orchestrator(options.poll_interval);
  while (keep_running) {
    orchestrator.poll_once();
    std::this_thread::sleep_for(options.poll_interval);
  }

  return 0;
}

#endif

#else

int run(AppShellOptions options) {
  rpc::log::init();
  rpc::core::Orchestrator orchestrator(options.poll_interval);
  while (true) {
    orchestrator.tick_once();
  }
  return 0;
}

#endif

} // namespace rpc::app
