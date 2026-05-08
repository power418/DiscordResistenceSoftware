module;

#include <cstdlib>
#include <iostream>

#if defined(_WIN32)
#  ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#  include <fcntl.h>
#  include <io.h>
#  include <windows.h>
#  include <shellapi.h>
#  include <cstdio>
#  include <string_view>
#  include <rpc/core/export.hpp>
#endif

export module rpc.utils.console;

export namespace rpc::console {

/**
 * @brief Enables the console window for logging.
 * 
 * In Debug mode, it is enabled by default.
 * In Release mode, it can be enabled via the --console or -c flags,
 * or by setting the SOFTWARE_RPC_CONSOLE=1 environment variable.
 */
RPC_CORE_API void enable() {
#if defined(_WIN32)
  const char* env_enabled = std::getenv("SOFTWARE_RPC_CONSOLE");
  bool force_console = false;

  // Check environment variable
  if (env_enabled != nullptr && env_enabled[0] == '1') {
    force_console = true;
  }

  // Check command line arguments
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv != nullptr) {
    for (int i = 0; i < argc; ++i) {
      std::wstring_view arg(argv[i]);
      if (arg == L"--console" || arg == L"-c") {
        force_console = true;
        break;
      }
    }
    LocalFree(argv);
  }

  bool is_debug = false;
#if !defined(NDEBUG)
  is_debug = true;
#endif

  // If not debug and not forced, we don't want a console
  if (!is_debug && !force_console) {
    return;
  }

  if (is_debug && env_enabled != nullptr && env_enabled[0] == '0') {
    return;
  }

  // Try to get a console window
  HWND console_window = GetConsoleWindow();
  if (console_window == nullptr) {
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
      if (!AllocConsole()) {
        return;
      }
    }
    console_window = GetConsoleWindow();
  }

  if (console_window != nullptr) {
    // Force the console window to be visible
    ShowWindow(console_window, SW_SHOW);
    SetForegroundWindow(console_window);

    // Redirect standard streams to ensure output goes to the console
    FILE* dummy = nullptr;
    (void)freopen_s(&dummy, "CONOUT$", "w", stdout);
    (void)freopen_s(&dummy, "CONOUT$", "w", stderr);
    (void)freopen_s(&dummy, "CONIN$", "r", stdin);

    // Clear stream states and set unbuffered mode
    std::cout.clear();
    std::cerr.clear();
    std::cin.clear();

    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);

    std::ios::sync_with_stdio(true);
  }
#endif
}

} // namespace rpc::console
