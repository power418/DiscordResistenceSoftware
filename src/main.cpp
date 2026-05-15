// #define _CRT_SECURE_NO_WARNINGS
#include <filesystem>

#include <rpc/rpc.hpp>

#if defined(_WIN32)
#  include <rpc/config/win32.h>
#  include <windows.h>
#endif

int main(void) {
  rpc::console::enable();

#if !defined(NDEBUG) && defined(_WIN32)
  {
    wchar_t module_path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, module_path, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
      const std::filesystem::path exe_dir = std::filesystem::path(module_path).parent_path();
      (void)rpc::load_dotenv(exe_dir / L".env");
    }
  }
#endif

#if !defined(NDEBUG)
  (void)rpc::load_dotenv();
#endif
  return rpc::app::run();
}

#if defined(_WIN32) && defined(NDEBUG)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  return main();
}
#endif
