// Pull in Windows UI baseline and manifest settings before module imports.
#include <rpc/config/win32.h>

import rpc;

int main() {
  rpc::load_dotenv();
  return rpc::app::run();
}
