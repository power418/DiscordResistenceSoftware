#include <rpc/platform/recent_activity_dialog.hpp>

#if defined(_WIN32)
#  include <algorithm>
#  include <string>
#  include <utility>

namespace {

constexpr wchar_t kRecentActivityDialogClassName[] =
  L"software_discord_rpc_recent_activity_dialog";
constexpr int kDialogMinWidth = 520;
constexpr int kDialogPreferredWidth = 620;
constexpr int kDialogMinHeight = 280;
constexpr int kDialogMargin = 24;
constexpr int kDialogGap = 12;
constexpr int kDialogButtonWidth = 96;
constexpr int kDialogButtonHeight = 34;

[[nodiscard]] HFONT create_font(int height, int weight) {
  return CreateFontW(
    height,
    0,
    0,
    0,
    weight,
    FALSE,
    FALSE,
    FALSE,
    DEFAULT_CHARSET,
    OUT_DEFAULT_PRECIS,
    CLIP_DEFAULT_PRECIS,
    DEFAULT_QUALITY,
    DEFAULT_PITCH,
    L"Segoe UI");
}

[[nodiscard]] int measure_text_height(HDC hdc,
                                      const std::wstring& text,
                                      HFONT font,
                                      int width) {
  if (text.empty() || width <= 0) {
    return 0;
  }

  RECT rect{0, 0, width, 0};
  const HGDIOBJ previous_font = SelectObject(hdc, font);
  DrawTextW(
    hdc,
    text.c_str(),
    static_cast<int>(text.size()),
    &rect,
    DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);
  SelectObject(hdc, previous_font);
  return rect.bottom - rect.top;
}

[[nodiscard]] bool point_in_rect(const RECT& rect, POINT point) {
  return point.x >= rect.left && point.x < rect.right &&
         point.y >= rect.top && point.y < rect.bottom;
}

[[nodiscard]] POINT point_from_lparam(LPARAM lparam) {
  return POINT{
    static_cast<LONG>(static_cast<short>(LOWORD(lparam))),
    static_cast<LONG>(static_cast<short>(HIWORD(lparam)))
  };
}

class RecentActivityDialog {
public:
  RecentActivityDialog(HWND owner,
                       HINSTANCE instance,
                       HICON icon,
                       std::wstring title,
                       std::wstring summary)
      : owner_(owner),
        instance_(instance),
        icon_(icon),
        title_(std::move(title)) {
    split_summary(std::move(summary));
  }

  int run() {
    if (!register_window_class()) {
      return 0;
    }

    const auto [window_width, window_height] = measure_window_size();
    const auto [x, y] = center_window(window_width, window_height);

    hwnd_ = CreateWindowExW(
      WS_EX_DLGMODALFRAME,
      kRecentActivityDialogClassName,
      title_.c_str(),
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
      x,
      y,
      window_width,
      window_height,
      owner_,
      nullptr,
      instance_,
      this);

    if (!hwnd_) {
      return 0;
    }

    if (icon_) {
      SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon_));
      SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon_));
    }

    rpc::platform::apply_window_theme(hwnd_);

    RECT client_rect{};
    GetClientRect(hwnd_, &client_rect);
    button_rect_ = compute_button_rect(client_rect);

    if (owner_ && IsWindow(owner_)) {
      EnableWindow(owner_, FALSE);
      owner_disabled_ = true;
    }

    ShowWindow(hwnd_, SW_SHOWNORMAL);
    UpdateWindow(hwnd_);
    SetForegroundWindow(hwnd_);

    MSG message{};
    while (!done_) {
      if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
          PostQuitMessage(static_cast<int>(message.wParam));
          break;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
      } else {
        WaitMessage();
      }
    }

    if (owner_disabled_ && owner_ && IsWindow(owner_)) {
      EnableWindow(owner_, TRUE);
      SetForegroundWindow(owner_);
    }

    hwnd_ = nullptr;
    return 0;
  }

private:
  HWND owner_ = nullptr;
  HINSTANCE instance_ = nullptr;
  HICON icon_ = nullptr;
  std::wstring title_;
  std::wstring header_;
  std::wstring body_;
  HWND hwnd_ = nullptr;
  RECT button_rect_{};
  bool owner_disabled_ = false;
  bool done_ = false;
  bool button_hot_ = false;
  bool button_pressed_ = false;

  void split_summary(std::wstring summary) {
    if (summary.empty()) {
      header_ = L"Recent activity (last 30 days)";
      body_ = L"No recent activity yet. Open a supported app to build history.";
      return;
    }

    const std::size_t separator = summary.find(L'\n');
    if (separator == std::wstring::npos) {
      header_ = std::move(summary);
      body_.clear();
      return;
    }

    header_ = summary.substr(0, separator);
    body_ = summary.substr(separator + 1);

    while (!body_.empty() && (body_.front() == L'\n' || body_.front() == L'\r')) {
      body_.erase(body_.begin());
    }

    if (body_.empty()) {
      body_ = L"No recent activity yet. Open a supported app to build history.";
    }
  }

  [[nodiscard]] std::pair<int, int> measure_window_size() const {
    const int screen_width = GetSystemMetrics(SM_CXSCREEN);
    const int screen_height = GetSystemMetrics(SM_CYSCREEN);
    const int available_width = std::max(kDialogMinWidth, screen_width - 80);
    const int client_width = std::clamp(kDialogPreferredWidth, kDialogMinWidth, available_width);

    HDC hdc = GetDC(nullptr);
    if (!hdc) {
      return {client_width, kDialogMinHeight};
    }

    const HFONT header_font = create_font(20, FW_SEMIBOLD);
    const HFONT body_font = create_font(16, FW_NORMAL);
    const int content_width = client_width - (kDialogMargin * 2);
    const int header_height = measure_text_height(hdc, header_, header_font, content_width);
    const int body_height = measure_text_height(hdc, body_, body_font, content_width);

    DeleteObject(header_font);
    DeleteObject(body_font);
    ReleaseDC(nullptr, hdc);

    const int client_height =
      kDialogMargin + header_height + kDialogGap + body_height +
      kDialogGap + kDialogButtonHeight + kDialogMargin;

    const int max_height = std::max(kDialogMinHeight, screen_height - 120);
    return {
      client_width,
      std::clamp(client_height, kDialogMinHeight, max_height),
    };
  }

  [[nodiscard]] std::pair<int, int> center_window(int window_width, int window_height) const {
    RECT owner_rect{};
    if (owner_ && IsWindow(owner_) && GetWindowRect(owner_, &owner_rect)) {
      const int x = owner_rect.left + ((owner_rect.right - owner_rect.left) - window_width) / 2;
      const int y = owner_rect.top + ((owner_rect.bottom - owner_rect.top) - window_height) / 2;
      return {std::max(0, x), std::max(0, y)};
    }

    const int x = (GetSystemMetrics(SM_CXSCREEN) - window_width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - window_height) / 2;
    return {std::max(0, x), std::max(0, y)};
  }

  [[nodiscard]] RECT compute_button_rect(const RECT& client_rect) const {
    RECT rect{};
    rect.right = client_rect.right - kDialogMargin;
    rect.left = rect.right - kDialogButtonWidth;
    rect.bottom = client_rect.bottom - kDialogMargin;
    rect.top = rect.bottom - kDialogButtonHeight;
    return rect;
  }

  [[nodiscard]] RECT compute_body_rect(const RECT& client_rect) const {
    RECT button_rect = compute_button_rect(client_rect);
    RECT rect{};
    rect.left = kDialogMargin;
    rect.top = kDialogMargin;
    rect.right = client_rect.right - kDialogMargin;
    rect.bottom = button_rect.top - kDialogGap;
    return rect;
  }

  void update_hover_state(POINT point) {
    const RECT current_button = button_rect_;
    const bool hot = point_in_rect(current_button, point);
    if (hot != button_hot_) {
      button_hot_ = hot;
      InvalidateRect(hwnd_, &button_rect_, TRUE);
    }
  }

  void close_dialog() {
    if (hwnd_) {
      DestroyWindow(hwnd_);
    }
  }

  void paint() const {
    PAINTSTRUCT paint_struct{};
    HDC hdc = BeginPaint(hwnd_, &paint_struct);

    RECT client_rect{};
    GetClientRect(hwnd_, &client_rect);
    const RECT body_rect = compute_body_rect(client_rect);
    const RECT button_rect = compute_button_rect(client_rect);

    const bool dark_mode = rpc::platform::is_system_dark_mode();
    const COLORREF background_color = dark_mode ? RGB(32, 34, 43) : RGB(246, 247, 250);
    const COLORREF header_color = dark_mode ? RGB(245, 245, 247) : RGB(22, 24, 29);
    const COLORREF body_color = dark_mode ? RGB(190, 198, 215) : RGB(79, 86, 104);
    const COLORREF button_background =
      dark_mode
        ? (button_pressed_
             ? RGB(57, 61, 74)
             : (button_hot_ ? RGB(48, 53, 65) : RGB(40, 44, 55)))
        : (button_pressed_
             ? RGB(219, 223, 232)
             : (button_hot_ ? RGB(229, 233, 240) : RGB(236, 238, 243)));
    const COLORREF button_border = dark_mode ? RGB(95, 101, 116) : RGB(198, 204, 214);
    const COLORREF button_text = dark_mode ? RGB(245, 245, 247) : RGB(22, 24, 29);

    HBRUSH background = CreateSolidBrush(background_color);
    FillRect(hdc, &client_rect, background);
    DeleteObject(background);

    SetBkMode(hdc, TRANSPARENT);

    const HFONT header_font = create_font(20, FW_SEMIBOLD);
    const HFONT body_font = create_font(16, FW_NORMAL);

    HFONT previous_font = static_cast<HFONT>(SelectObject(hdc, header_font));
    SetTextColor(hdc, header_color);
    RECT header_rect = body_rect;
    DrawTextW(
      hdc,
      header_.c_str(),
      static_cast<int>(header_.size()),
      &header_rect,
      DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);

    SelectObject(hdc, body_font);
    SetTextColor(hdc, body_color);
    RECT text_rect = body_rect;
    text_rect.top += measure_text_height(hdc, header_, header_font, body_rect.right - body_rect.left);
    text_rect.top += kDialogGap;
    DrawTextW(
      hdc,
      body_.c_str(),
      static_cast<int>(body_.size()),
      &text_rect,
      DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);

    HBRUSH button_brush = CreateSolidBrush(button_background);
    FillRect(hdc, &button_rect, button_brush);
    DeleteObject(button_brush);

    HBRUSH border_brush = CreateSolidBrush(button_border);
    FrameRect(hdc, &button_rect, border_brush);
    DeleteObject(border_brush);

    RECT button_text_rect = button_rect;
    SetTextColor(hdc, button_text);
    DrawTextW(
      hdc,
      L"OK",
      -1,
      &button_text_rect,
      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, previous_font);
    DeleteObject(header_font);
    DeleteObject(body_font);

    EndPaint(hwnd_, &paint_struct);
  }

  bool register_window_class() const {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &RecentActivityDialog::window_proc;
    window_class.hInstance = instance_;
    window_class.hIcon = icon_ ? icon_ : LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.hbrBackground = nullptr;
    window_class.lpszClassName = kRecentActivityDialogClassName;
    window_class.hIconSm = window_class.hIcon;

    return RegisterClassExW(&window_class) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }

  static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<RecentActivityDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
      self = static_cast<RecentActivityDialog*>(create_struct->lpCreateParams);
      self->hwnd_ = hwnd;
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (!self) {
      return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    switch (message) {
      case WM_SIZE:
        self->button_rect_ = self->compute_button_rect(
          RECT{0, 0, LOWORD(lparam), HIWORD(lparam)});
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

      case WM_ERASEBKGND:
        return 1;

      case WM_PAINT:
        self->paint();
        return 0;

      case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT track_mouse_event{};
        track_mouse_event.cbSize = sizeof(track_mouse_event);
        track_mouse_event.dwFlags = TME_LEAVE;
        track_mouse_event.hwndTrack = hwnd;
        TrackMouseEvent(&track_mouse_event);

        const POINT point = point_from_lparam(lparam);
        self->update_hover_state(point);
        return 0;
      }

      case WM_MOUSELEAVE:
        if (self->button_hot_) {
          self->button_hot_ = false;
          InvalidateRect(hwnd, &self->button_rect_, TRUE);
        }
        return 0;

      case WM_LBUTTONDOWN: {
        const POINT point = point_from_lparam(lparam);
        if (point_in_rect(self->button_rect_, point)) {
          self->button_pressed_ = true;
          SetCapture(hwnd);
          InvalidateRect(hwnd, &self->button_rect_, TRUE);
        }
        return 0;
      }

      case WM_LBUTTONUP: {
        const POINT point = point_from_lparam(lparam);
        const bool was_pressed = self->button_pressed_;
        self->button_pressed_ = false;
        ReleaseCapture();
        InvalidateRect(hwnd, &self->button_rect_, TRUE);
        if (was_pressed && point_in_rect(self->button_rect_, point)) {
          self->close_dialog();
        }
        return 0;
      }

      case WM_KEYDOWN:
        if (wparam == VK_ESCAPE || wparam == VK_RETURN) {
          self->close_dialog();
          return 0;
        }
        break;

      case WM_CLOSE:
        self->close_dialog();
        return 0;

      case WM_DESTROY:
        self->done_ = true;
        return 0;

      case WM_SETTINGCHANGE:
      case WM_THEMECHANGED:
        rpc::platform::apply_window_theme(hwnd);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
  }
};

} // namespace

namespace rpc::platform {

void show_recent_activity_dialog(HWND owner,
                                 HINSTANCE instance,
                                 HICON icon,
                                 std::wstring_view title,
                                 std::wstring_view summary) {
  RecentActivityDialog dialog(
    owner,
    instance,
    icon,
    std::wstring(title),
    std::wstring(summary));
  dialog.run();
}

} // namespace rpc::platform

#endif
