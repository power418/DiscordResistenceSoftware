module;

#include <chrono>
#include <string>
#include <string_view>
#include <thread>

export module rpc.core.orchestrator;

import rpc.activity;
import rpc.core;
import rpc.detectors.creative_apps;
import rpc.discord.ipc_client;
import rpc.os.active_window;
import rpc.os.icon_cache;
import rpc.utils.logger;

export namespace rpc::core {

class Orchestrator {
public:
  explicit Orchestrator(std::chrono::milliseconds poll_interval = std::chrono::milliseconds(1000))
      : poll_interval_(poll_interval) {}

  void poll_once() const {
    if (rpc::sync_dotenv_if_changed()) {
      rpc::log::info(".env synced: {}", rpc::dotenv_path().string());
    }

    // Log active window changes (for debugging)
    const auto snapshot = rpc::query_active_window();
    if (!snapshot.process_name.empty() && snapshot.process_name != last_seen_process_) {
      last_seen_process_ = snapshot.process_name;
      rpc::log::debug("Active window: {}", snapshot.process_name);
    }

    // ── Sticky RPC logic ──
    // 1. If active window is a creative app → adopt it (update/switch)
    // 2. If active window is NOT creative → keep existing RPC as long as its process lives
    // 3. Only clear when the sticky process actually exits

    const auto detected = rpc::detectors::detect_creative_activity(snapshot);

    if (detected.has_value()) {
      // Active window IS a creative app → adopt or keep it
      adopt_creative_app(snapshot, detected.value());
      return;
    }

    // Active window is NOT a creative app — keep sticky RPC if process is alive
    if (!sticky_process_.empty()) {
      if (rpc::is_process_running(sticky_process_)) {
        // Process still alive → RPC stays, do nothing
        ensure_activity_sent();
        return;
      }

      // Sticky process has exited → clear RPC
      rpc::log::info("{} has exited, clearing Discord activity", sticky_process_);
      clear_activity();
      sticky_process_.clear();
      sticky_exe_path_.clear();
      sticky_activity_ = {};
      session_start_ = 0;
    }
  }

  void tick_once() const {
    poll_once();
    std::this_thread::sleep_for(poll_interval_);
  }

private:
  std::chrono::milliseconds poll_interval_;
  mutable rpc::discord::IpcClient discord_;
  mutable rpc::IconCache icon_cache_;

  // Sticky state — the creative app whose RPC stays active
  mutable std::string sticky_process_;          // e.g. "FL64.exe"
  mutable std::string sticky_exe_path_;         // full path for icon extraction
  mutable rpc::ActivityPayload sticky_activity_;
  mutable std::uint64_t session_start_ = 0;

  // Misc state
  mutable std::string last_seen_process_;
  mutable std::string last_activity_key_;
  mutable std::string last_large_image_;
  mutable bool activity_is_set_ = false;
  mutable bool warned_missing_client_id_ = false;
  mutable bool warned_connect_failure_ = false;
  mutable std::chrono::steady_clock::time_point next_connect_attempt_{};

  // ── Core logic ──

  void adopt_creative_app(const rpc::ActiveWindowInfo& snapshot,
                          const rpc::ActivityPayload& detected) const {
    const std::string activity_key = detected.details + "\n" + detected.state;

    // Detect changes
    const bool is_different_app = (sticky_process_ != snapshot.process_name);
    const bool is_title_changed = (last_activity_key_ != activity_key);

    if (is_different_app) {
      // Switching to a different creative app → new session
      rpc::log::info("Creative app detected: {} → {} / {}",
                     snapshot.process_name, detected.details, detected.state);
      sticky_process_ = snapshot.process_name;
      sticky_exe_path_ = snapshot.exe_path;
      session_start_ = rpc::unix_timestamp_seconds_now();
    } else if (is_title_changed && session_start_ == 0) {
      // Same app but no session yet
      session_start_ = rpc::unix_timestamp_seconds_now();
    }
    // Otherwise: same app, title may have changed → keep session_start_ as-is

    if (is_title_changed) {
      rpc::log::info("RPC details: {} ({})", detected.details, detected.state);
    }

    // Build the activity payload
    sticky_activity_ = detected;
    sticky_activity_.start_timestamp_unix = session_start_;

    // Auto-detect icon from the active window first, then fall back to the exe.
    std::string icon_url = icon_cache_.resolve_icon_url(snapshot.window_handle, sticky_exe_path_);
    const std::string fallback_large_image = rpc::env_or("DISCORD_LARGE_IMAGE", "");
    sticky_activity_.large_image = icon_url.empty() ? fallback_large_image : icon_url;
    sticky_activity_.large_text = rpc::env_or("DISCORD_LARGE_TEXT", detected.state);

    const bool is_large_image_changed = (sticky_activity_.large_image != last_large_image_);

    // Send to Discord when something changed
    const bool need_update = is_different_app || is_title_changed
                          || is_large_image_changed
                          || !activity_is_set_ || !discord_.connected();
    if (need_update) {
      send_activity(activity_key);
    }
  }

  void ensure_activity_sent() const {
    // Re-send sticky activity if Discord reconnected or activity was lost
    if (activity_is_set_ && discord_.connected()) {
      return;
    }

    if (sticky_activity_.details.empty()) {
      return;
    }

    const std::string key = sticky_activity_.details + "\n" + sticky_activity_.state;
    send_activity(key);
  }

  void send_activity(const std::string& activity_key) const {
    if (!ensure_discord_connected()) {
      return;
    }

    if (discord_.set_activity(sticky_activity_)) {
      last_activity_key_ = activity_key;
      last_large_image_ = sticky_activity_.large_image;
      activity_is_set_ = true;
      rpc::log::info("Discord RPC active: {} / {} (session since {})",
                     sticky_activity_.details, sticky_activity_.state, session_start_);
      return;
    }

    rpc::log::warn("Discord activity update failed: {}", discord_.last_error());
    next_connect_attempt_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  }

  void clear_activity() const {
    if (!activity_is_set_) {
      return;
    }

    if (ensure_discord_connected() && discord_.clear_activity()) {
      rpc::log::info("Discord RPC cleared");
    }

    last_activity_key_.clear();
    last_large_image_.clear();
    activity_is_set_ = false;
  }

  // ── Discord connection ──

  [[nodiscard]] bool ensure_discord_connected() const {
    if (discord_.connected()) {
      return true;
    }
    if (std::chrono::steady_clock::now() < next_connect_attempt_) {
      return false;
    }

    const std::string discord_client_id = rpc::client_id();
    if (discord_client_id.empty()) {
      if (!warned_missing_client_id_) {
        rpc::log::warn("DISCORD_CLIENT_ID is empty; Rich Presence update skipped");
        warned_missing_client_id_ = true;
      }
      return false;
    }

    if (!discord_.connect(discord_client_id)) {
      if (!warned_connect_failure_) {
        rpc::log::warn("Discord IPC connect failed: {}", discord_.last_error());
        if (discord_.last_error().find("Invalid Client ID") != std::string_view::npos) {
          rpc::log::warn("Use Discord Developer Portal Application ID for DISCORD_CLIENT_ID");
        }
        warned_connect_failure_ = true;
      }
      next_connect_attempt_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      return false;
    }

    next_connect_attempt_ = {};
    warned_connect_failure_ = false;
    rpc::log::info("Connected to Discord IPC");
    return true;
  }
};

} // namespace rpc::core
