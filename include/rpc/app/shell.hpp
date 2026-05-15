#pragma once

#include <chrono>

#include <rpc/core/export.hpp>

namespace rpc::app {

struct AppShellOptions {
  std::chrono::milliseconds splash_duration = std::chrono::milliseconds(2500);
  std::chrono::milliseconds poll_interval = std::chrono::milliseconds(1000);
  bool show_splash_on_start = true;
  bool enable_tray = true;
};

RPC_CORE_API int run(AppShellOptions options = {});

} // namespace rpc::app
