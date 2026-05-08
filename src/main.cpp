// Pull in Windows UI baseline and manifest settings before module imports.
#include <rpc/config/win32.h>

#if defined(_WIN32) && defined(NDEBUG)
#  include <windows.h>
#endif

import rpc;

int main() {
  rpc::load_dotenv();
  return rpc::app::run();
}

#if defined(_WIN32) && defined(NDEBUG)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  return main();
}
#endif
