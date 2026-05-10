module;

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <optional>
#include <thread>

export module rpc.core.orchestrator;

import rpc.activity;
import rpc.core;
import rpc.config;
import rpc.detectors.creative_apps;
import rpc.detectors.office_apps;
import rpc.detectors.productive_apps;
import rpc.core.recent_activity;
import rpc.discord.ipc_client;
import rpc.os.active_window;
import rpc.os.icon_cache;
import rpc.utils.logger;

export namespace rpc::core {

class Orchestrator {
public:
  explicit Orchestrator(std::chrono::milliseconds poll_interval = std::chrono::milliseconds(1000))
      : poll_interval_(poll_interval) {}

  ~Orchestrator();

  void poll_once() const;
  void tick_once() const;

  [[nodiscard]] std::string recent_activity_summary(std::size_t limit = 10) const;

  void save_history() const;

private:
  std::chrono::milliseconds poll_interval_;
  mutable rpc::discord::IpcClient discord_;
  mutable rpc::IconCache icon_cache_;
  mutable rpc::RecentActivityStore recent_activity_;

  mutable std::string sticky_process_;
  mutable std::string sticky_exe_path_;
  mutable rpc::ActivityPayload sticky_activity_;
  mutable std::uint64_t session_start_ = 0;

  mutable std::string last_seen_process_;
  mutable std::string last_history_key_;
  mutable std::string last_activity_key_;
  mutable std::string last_large_image_;
  mutable bool activity_is_set_ = false;
  mutable bool warned_missing_client_id_ = false;
  mutable bool warned_connect_failure_ = false;
  mutable std::chrono::steady_clock::time_point next_connect_attempt_{};

  void adopt_creative_app(const rpc::ActiveWindowInfo& snapshot,
                          const rpc::ActivityPayload& detected) const;

  void ensure_activity_sent() const;
  void send_activity(const std::string& activity_key) const;
  void clear_activity() const;

  [[nodiscard]] bool ensure_discord_connected() const;
};

} // namespace rpc::core
