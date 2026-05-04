module;

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <system_error>
#include <string>
#include <string_view>
#include <vector>

#include <chrono>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <tlhelp32.h>
#elif defined(__linux__) && defined(SOFTWARE_RPC_HAS_X11)
#  include <X11/Xatom.h>
#  include <X11/Xlib.h>
#  include <X11/Xutil.h>
#endif

export module rpc.os.active_window;

export namespace rpc {

struct ActiveWindowInfo {
  std::string title;
  std::string process_name;
  std::string exe_path;
  std::uintptr_t window_handle = 0;
  std::uint64_t start_timestamp_unix = 0;
};

[[nodiscard]] inline std::uint64_t unix_timestamp_seconds_now() {
  using namespace std::chrono;
  return static_cast<std::uint64_t>(
    duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

#if defined(_WIN32)
[[nodiscard]] inline std::string wide_to_utf8(std::wstring_view input) {
  if (input.empty()) {
    return {};
  }
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, input.data(),
                                        static_cast<int>(input.size()), nullptr, 0,
                                        nullptr, nullptr);
  if (size_needed <= 0) {
    return {};
  }
  std::string output(static_cast<std::size_t>(size_needed), '\0');
  WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
                      output.data(), size_needed, nullptr, nullptr);
  return output;
}

[[nodiscard]] inline std::wstring get_window_title(HWND hwnd) {
  int length = GetWindowTextLengthW(hwnd);
  if (length <= 0) {
    return {};
  }

  std::wstring buffer(static_cast<std::size_t>(length) + 1, L'\0');
  int written = GetWindowTextW(hwnd, buffer.data(), static_cast<int>(buffer.size()));
  if (written <= 0) {
    return {};
  }
  buffer.resize(static_cast<std::size_t>(written));
  return buffer;
}

[[nodiscard]] inline std::wstring get_process_image_path(DWORD pid) {
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!process) {
    return {};
  }

  std::vector<wchar_t> buffer(512, L'\0');
  DWORD size = static_cast<DWORD>(buffer.size());

  for (int attempt = 0; attempt < 4; ++attempt) {
    if (QueryFullProcessImageNameW(process, 0, buffer.data(), &size) != 0) {
      CloseHandle(process);
      return std::wstring(buffer.data(), buffer.data() + size);
    }
    buffer.resize(buffer.size() * 2);
    size = static_cast<DWORD>(buffer.size());
  }

  CloseHandle(process);
  return {};
}
#endif

#if defined(__linux__) && defined(SOFTWARE_RPC_HAS_X11)
[[nodiscard]] inline std::string get_window_property_string(Display* display,
                                                            Window window,
                                                            Atom property,
                                                            Atom type) {
  Atom actual_type = None;
  int actual_format = 0;
  unsigned long item_count = 0;
  unsigned long bytes_after = 0;
  unsigned char* property_data = nullptr;

  const int status = XGetWindowProperty(display, window, property, 0, 1024, False, type,
                                        &actual_type, &actual_format, &item_count,
                                        &bytes_after, &property_data);
  if (status != Success || property_data == nullptr) {
    return {};
  }

  std::string value;
  if (actual_format == 8 && item_count > 0) {
    value.assign(reinterpret_cast<char*>(property_data), item_count);
  }
  XFree(property_data);
  return value;
}

[[nodiscard]] inline Window get_active_x11_window(Display* display) {
  const Atom active_window_atom = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
  if (active_window_atom == None) {
    return None;
  }

  Atom actual_type = None;
  int actual_format = 0;
  unsigned long item_count = 0;
  unsigned long bytes_after = 0;
  unsigned char* property_data = nullptr;

  const int status = XGetWindowProperty(display, DefaultRootWindow(display), active_window_atom,
                                        0, 1, False, XA_WINDOW, &actual_type,
                                        &actual_format, &item_count, &bytes_after,
                                        &property_data);
  if (status != Success || property_data == nullptr || item_count == 0) {
    if (property_data != nullptr) {
      XFree(property_data);
    }
    return None;
  }

  const Window active_window = *reinterpret_cast<Window*>(property_data);
  XFree(property_data);
  return active_window;
}

[[nodiscard]] inline std::string get_x11_window_title(Display* display, Window window) {
  const Atom utf8_atom = XInternAtom(display, "UTF8_STRING", True);
  const Atom net_wm_name_atom = XInternAtom(display, "_NET_WM_NAME", True);

  if (utf8_atom != None && net_wm_name_atom != None) {
    std::string title = get_window_property_string(display, window, net_wm_name_atom, utf8_atom);
    if (!title.empty()) {
      return title;
    }
  }

  char* fallback_title = nullptr;
  if (XFetchName(display, window, &fallback_title) > 0 && fallback_title != nullptr) {
    std::string title = fallback_title;
    XFree(fallback_title);
    return title;
  }

  return {};
}

[[nodiscard]] inline unsigned long get_x11_window_pid(Display* display, Window window) {
  const Atom pid_atom = XInternAtom(display, "_NET_WM_PID", True);
  if (pid_atom == None) {
    return 0;
  }

  Atom actual_type = None;
  int actual_format = 0;
  unsigned long item_count = 0;
  unsigned long bytes_after = 0;
  unsigned char* property_data = nullptr;

  const int status = XGetWindowProperty(display, window, pid_atom, 0, 1, False, XA_CARDINAL,
                                        &actual_type, &actual_format, &item_count,
                                        &bytes_after, &property_data);
  if (status != Success || property_data == nullptr || item_count == 0) {
    if (property_data != nullptr) {
      XFree(property_data);
    }
    return 0;
  }

  const auto pid = *reinterpret_cast<unsigned long*>(property_data);
  XFree(property_data);
  return pid;
}

[[nodiscard]] inline std::string get_x11_class_name(Display* display, Window window) {
  XClassHint class_hint{};
  if (XGetClassHint(display, window, &class_hint) == 0) {
    return {};
  }

  std::string class_name;
  if (class_hint.res_class != nullptr) {
    class_name = class_hint.res_class;
  } else if (class_hint.res_name != nullptr) {
    class_name = class_hint.res_name;
  }

  if (class_hint.res_name != nullptr) {
    XFree(class_hint.res_name);
  }
  if (class_hint.res_class != nullptr) {
    XFree(class_hint.res_class);
  }

  return class_name;
}
#endif

[[nodiscard]] inline ActiveWindowInfo query_active_window() {
  ActiveWindowInfo info{};
  info.start_timestamp_unix = unix_timestamp_seconds_now();

#if defined(_WIN32)
  HWND hwnd = GetForegroundWindow();
  if (!hwnd) {
    return info;
  }

  info.window_handle = reinterpret_cast<std::uintptr_t>(hwnd);
  info.title = wide_to_utf8(get_window_title(hwnd));

  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid == 0) {
    return info;
  }

  std::wstring image_path = get_process_image_path(pid);
  info.exe_path = wide_to_utf8(image_path);
  if (!info.exe_path.empty()) {
    info.process_name = std::filesystem::path(info.exe_path).filename().string();
  }
#elif defined(__linux__) && defined(SOFTWARE_RPC_HAS_X11)
  Display* display = XOpenDisplay(nullptr);
  if (display == nullptr) {
    return info;
  }

  const Window active_window = get_active_x11_window(display);
  if (active_window == None) {
    XCloseDisplay(display);
    return info;
  }

  info.title = get_x11_window_title(display, active_window);

  const unsigned long pid = get_x11_window_pid(display, active_window);
  if (pid != 0) {
    std::error_code error;
    const auto exe_path = std::filesystem::read_symlink(
      std::filesystem::path("/proc") / std::to_string(pid) / "exe", error);
    if (!error) {
      info.exe_path = exe_path.string();
      info.process_name = exe_path.filename().string();
    }
  }

  if (info.process_name.empty()) {
    info.process_name = get_x11_class_name(display, active_window);
  }

  XCloseDisplay(display);
#endif

  return info;
}

// ---------------------------------------------------------------------------
// Check if a process with the given name is still running
// ---------------------------------------------------------------------------

[[nodiscard]] inline bool is_process_running(std::string_view process_name) {
  if (process_name.empty()) {
    return false;
  }

#if defined(_WIN32)
  auto iequals = [](std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
      if (std::tolower(static_cast<unsigned char>(a[i])) !=
          std::tolower(static_cast<unsigned char>(b[i]))) {
        return false;
      }
    }
    return true;
  };

  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return false;
  }

  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);

  if (Process32FirstW(snapshot, &entry) == FALSE) {
    CloseHandle(snapshot);
    return false;
  }

  do {
    std::string current_name = wide_to_utf8(entry.szExeFile);
    if (iequals(current_name, process_name)) {
      CloseHandle(snapshot);
      return true;
    }
  } while (Process32NextW(snapshot, &entry) != FALSE);

  CloseHandle(snapshot);
  return false;

#elif defined(__linux__)
  std::error_code error;
  for (const auto& dir_entry : std::filesystem::directory_iterator("/proc", error)) {
    if (!dir_entry.is_directory()) continue;
    const auto& pid_path = dir_entry.path();
    const auto pid_str = pid_path.filename().string();
    if (pid_str.empty() || !std::isdigit(static_cast<unsigned char>(pid_str[0]))) continue;

    std::error_code link_error;
    const auto exe = std::filesystem::read_symlink(pid_path / "exe", link_error);
    if (link_error) continue;

    if (exe.filename().string() == process_name) {
      return true;
    }
  }
  return false;

#else
  (void)process_name;
  return false;
#endif
}

} // namespace rpc
