#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <string>
#include <vector>
#include <windows.h>

#include <rpc/platform/settings_dialog.hpp>
#include <rpc/platform/theme.hpp>

import rpc.config;
import rpc.os.autostart;

namespace {

constexpr wchar_t kSettingsDialogClassName[] =
    L"software_discord_rpc_settings_dialog";
constexpr int kDialogWidth = 460;
constexpr int kDialogHeight = 320;
constexpr int kMargin = 24;
constexpr int kItemHeight = 40;
constexpr int kCheckboxSize = 18;
constexpr int kButtonWidth = 96;
constexpr int kButtonHeight = 34;

struct SettingItem {
  std::wstring label;
  RECT rect;
  bool *value;
  bool is_autostart = false;
};

class SettingsDialog {
public:
  SettingsDialog(HWND owner, HINSTANCE instance, HICON icon,
                 rpc::Config &config)
      : owner_(owner), instance_(instance), icon_(icon), config_(config) {
    items_ = {
        {L"Run at startup", {}, &config_.autostart, true},
        {L"Generic Mode (Productive Apps)", {}, &config_.generic_mode, false},
        {L"Show File/Project Name", {}, &config_.show_file_name, false}};
  }

  void run() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance_;
    wc.hIcon = icon_;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kSettingsDialogClassName;
    RegisterClassExW(&wc);

    const int x = (GetSystemMetrics(SM_CXSCREEN) - kDialogWidth) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - kDialogHeight) / 2;

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST, kSettingsDialogClassName,
        L"Settings", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x, y,
        kDialogWidth, kDialogHeight, owner_, nullptr, instance_, this);

    if (!hwnd_)
      return;

    SetWindowTextW(hwnd_, L"Settings");

    if (icon_) {
      SendMessageW(hwnd_, WM_SETICON, ICON_BIG,
                   reinterpret_cast<LPARAM>(icon_));
      SendMessageW(hwnd_, WM_SETICON, ICON_SMALL,
                   reinterpret_cast<LPARAM>(icon_));
    }

    rpc::platform::apply_window_theme(hwnd_);

    if (owner_ && IsWindow(owner_))
      EnableWindow(owner_, FALSE);

    ShowWindow(hwnd_, SW_SHOWNORMAL);
    UpdateWindow(hwnd_);

    MSG msg{};
    while (!done_) {
      if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT)
          break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      } else {
        WaitMessage();
      }
    }

    if (owner_ && IsWindow(owner_)) {
      EnableWindow(owner_, TRUE);
      SetForegroundWindow(owner_);
    }
  }

private:
  HWND owner_;
  HINSTANCE instance_;
  HICON icon_;
  rpc::Config &config_;
  HWND hwnd_{};
  std::vector<SettingItem> items_;
  RECT ok_button_rect_{};
  bool done_ = false;
  int hovered_item_ = -1; // -1: none, 0-2: items, 3: OK button
  bool button_pressed_ = false;

  void update_rects() {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int top = kMargin + 60;
    for (auto &item : items_) {
      item.rect = {kMargin, top, rc.right - kMargin, top + kItemHeight};
      top += kItemHeight;
    }
    ok_button_rect_ = {rc.right - kMargin - kButtonWidth,
                       rc.bottom - kMargin - kButtonHeight, rc.right - kMargin,
                       rc.bottom - kMargin};
  }

  void paint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);
    RECT rc;
    GetClientRect(hwnd_, &rc);

    const bool dark = rpc::platform::is_system_dark_mode();
    const COLORREF bg_color = dark ? RGB(32, 34, 43) : RGB(246, 247, 250);
    const COLORREF text_color = dark ? RGB(245, 245, 247) : RGB(22, 24, 29);
    const COLORREF btn_bg =
        dark ? (button_pressed_ && hovered_item_ == 3
                    ? RGB(57, 61, 74)
                    : (hovered_item_ == 3 ? RGB(48, 53, 65) : RGB(40, 44, 55)))
             : (button_pressed_ && hovered_item_ == 3
                    ? RGB(219, 223, 232)
                    : (hovered_item_ == 3 ? RGB(229, 233, 240)
                                          : RGB(236, 238, 243)));
    const COLORREF btn_border = dark ? RGB(95, 101, 116) : RGB(198, 204, 214);

    HBRUSH bg_br = CreateSolidBrush(bg_color);
    FillRect(hdc, &rc, bg_br);
    DeleteObject(bg_br);

    // Draw Title
    HFONT title_font =
        CreateFontW(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT old_font = (HFONT)SelectObject(hdc, title_font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, text_color);
    RECT title_rc = {kMargin, kMargin, rc.right - kMargin, kMargin + 40};
    DrawTextW(hdc, L"Settings", -1, &title_rc,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old_font);
    DeleteObject(title_font);

    HFONT font =
        CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    old_font = (HFONT)SelectObject(hdc, font);

    for (int i = 0; i < (int)items_.size(); ++i) {
      const auto &item = items_[i];
      bool val =
          item.is_autostart ? rpc::os::is_autostart_enabled() : *item.value;

      // Draw checkbox
      RECT cb_rc = {item.rect.left,
                    item.rect.top + (kItemHeight - kCheckboxSize) / 2,
                    item.rect.left + kCheckboxSize,
                    item.rect.top + (kItemHeight + kCheckboxSize) / 2};

      HBRUSH cb_br =
          CreateSolidBrush(dark ? RGB(45, 48, 60) : RGB(255, 255, 255));
      FillRect(hdc, &cb_rc, cb_br);
      DeleteObject(cb_br);

      HBRUSH border_br =
          CreateSolidBrush(dark ? RGB(100, 105, 120) : RGB(180, 185, 200));
      FrameRect(hdc, &cb_rc, border_br);
      DeleteObject(border_br);

      if (val) {
        // Draw check mark (simplified as a small filled rect)
        RECT inner = {cb_rc.left + 4, cb_rc.top + 4, cb_rc.right - 4,
                      cb_rc.bottom - 4};
        HBRUSH check_br =
            CreateSolidBrush(dark ? RGB(0, 120, 215) : RGB(0, 100, 200));
        FillRect(hdc, &inner, check_br);
        DeleteObject(check_br);
      }

      // Draw text
      RECT text_rc = item.rect;
      text_rc.left += kCheckboxSize + 12;
      SetTextColor(hdc, text_color);
      DrawTextW(hdc, item.label.c_str(), -1, &text_rc,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // Draw OK button
    HBRUSH btn_br = CreateSolidBrush(btn_bg);
    FillRect(hdc, &ok_button_rect_, btn_br);
    DeleteObject(btn_br);

    HBRUSH btn_border_br = CreateSolidBrush(btn_border);
    FrameRect(hdc, &ok_button_rect_, btn_border_br);
    DeleteObject(btn_border_br);

    SetTextColor(hdc, text_color);
    DrawTextW(hdc, L"OK", -1, &ok_button_rect_,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, old_font);
    DeleteObject(font);
    EndPaint(hwnd_, &ps);
  }

  static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp,
                                      LPARAM lp) {
    auto *self = (SettingsDialog *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_NCCREATE: {
      auto *cs = (CREATESTRUCTW *)lp;
      self = (SettingsDialog *)cs->lpCreateParams;
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
      return TRUE;
    }
    case WM_CREATE:
      return 0;
    case WM_SIZE:
      if (self)
        self->update_rects();
      return 0;
    case WM_PAINT:
      if (self)
        self->paint();
      return 0;
    case WM_MOUSEMOVE: {
      if (!self)
        return 0;
      POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
      int prev_hover = self->hovered_item_;
      self->hovered_item_ = -1;
      for (int i = 0; i < (int)self->items_.size(); ++i) {
        if (PtInRect(&self->items_[i].rect, pt)) {
          self->hovered_item_ = i;
          break;
        }
      }
      if (self->hovered_item_ == -1 && PtInRect(&self->ok_button_rect_, pt)) {
        self->hovered_item_ = 3;
      }
      if (prev_hover != self->hovered_item_)
        InvalidateRect(hwnd, nullptr, TRUE);
      return 0;
    }
    case WM_LBUTTONDOWN: {
      if (!self)
        return 0;
      POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
      if (PtInRect(&self->ok_button_rect_, pt)) {
        self->button_pressed_ = true;
        InvalidateRect(hwnd, &self->ok_button_rect_, TRUE);
      } else {
        for (int i = 0; i < (int)self->items_.size(); ++i) {
          if (PtInRect(&self->items_[i].rect, pt)) {
            if (self->items_[i].is_autostart) {
              bool enabled = !rpc::os::is_autostart_enabled();
              rpc::os::set_autostart_enabled(enabled);
              self->config_.autostart = enabled;
            } else {
              *self->items_[i].value = !(*self->items_[i].value);
            }
            rpc::save_config(rpc::settings_path(), self->config_);
            InvalidateRect(hwnd, nullptr, TRUE);
            break;
          }
        }
      }
      return 0;
    }
    case WM_LBUTTONUP: {
      if (!self)
        return 0;
      if (self->button_pressed_) {
        self->button_pressed_ = false;
        POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
        if (PtInRect(&self->ok_button_rect_, pt)) {
          DestroyWindow(hwnd);
        } else {
          InvalidateRect(hwnd, &self->ok_button_rect_, TRUE);
        }
      }
      return 0;
    }
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      if (self)
        self->done_ = true;
      return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
  }
};

} // namespace

namespace rpc::platform {
void show_settings_dialog(HWND owner, HINSTANCE instance, HICON icon,
                          rpc::Config &config) {
  SettingsDialog dlg(owner, instance, icon, config);
  dlg.run();
}
} // namespace rpc::platform

#endif
