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

    const auto snapshot = rpc::query_active_window();
    if (!snapshot.process_name.empty() && snapshot.process_name != last_seen_process_) {
      last_seen_process_ = snapshot.process_name;
      rpc::log::info("Active window: {}", snapshot.process_name);
    }
    publish_activity(snapshot);
  }

  void tick_once() const {
    poll_once();
    std::this_thread::sleep_for(poll_interval_);
  }

private:
  std::chrono::milliseconds poll_interval_;
  mutable rpc::discord::IpcClient discord_;
  mutable std::string last_activity_key_;
  mutable std::string last_seen_process_;
  mutable std::string last_focus_log_key_;
  mutable std::uint64_t session_start_ = 0;
  mutable std::string last_creative_app_key_;               // tracks which creative app session is active
  mutable std::chrono::steady_clock::time_point last_creative_seen_{}; // when we last saw a creative app
  mutable bool activity_is_set_ = false;                     // whether Discord currently shows an activity
  mutable bool warned_missing_client_id_ = false;
  mutable bool warned_connect_failure_ = false;
  mutable std::chrono::steady_clock::time_point next_connect_attempt_{};

  // How long we tolerate being away from a creative app before resetting the timer
  static constexpr auto kSessionTimeout = std::chrono::seconds(30);

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

  void publish_activity(const rpc::ActiveWindowInfo& snapshot) const {
    const auto detected_activity = rpc::detectors::detect_creative_activity(snapshot);
    if (!detected_activity.has_value()) {
      log_creative_focus_miss(snapshot);
      clear_activity_if_needed();
      return;
    }

    rpc::ActivityPayload activity = detected_activity.value();
    log_creative_focus_match(snapshot, activity);

    const std::string activity_key = activity.details + "\n" + activity.state;
    const auto now = std::chrono::steady_clock::now();

    // Update the "last time we saw a creative app" timestamp
    last_creative_seen_ = now;

    // Decide whether to start a fresh session or continue the existing one
    if (activity_key != last_creative_app_key_) {
      // Different creative app (e.g. FL Studio -> Krita): start new session
      session_start_ = rpc::unix_timestamp_seconds_now();
      last_creative_app_key_ = activity_key;
    } else if (session_start_ == 0) {
      // Same creative app but session was expired: start new session
      session_start_ = rpc::unix_timestamp_seconds_now();
    }
    // Otherwise: same creative app, session still valid -> keep session_start_ as-is

    activity.start_timestamp_unix = session_start_;

    // Set the large image asset from .env (upload it on Discord Developer Portal first)
    activity.large_image = rpc::env_or("DISCORD_LARGE_IMAGE", "");
    activity.large_text = rpc::env_or("DISCORD_LARGE_TEXT", activity.state);

    // Always re-send the activity when coming back from a non-creative app,
    // because we cleared it when we left. Also re-send if discord reconnected.
    const bool need_update = !activity_is_set_
                          || activity_key != last_activity_key_
                          || !discord_.connected();

    if (!need_update) {
      return;
    }

    if (!ensure_discord_connected()) {
      return;
    }

    if (discord_.set_activity(activity)) {
      last_activity_key_ = activity_key;
      activity_is_set_ = true;
      rpc::log::info("Discord activity updated: {} / {} (elapsed since {})",
                     activity.details, activity.state, session_start_);
      return;
    }

    rpc::log::warn("Discord activity update failed: {}", discord_.last_error());
    next_connect_attempt_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  }

  void clear_activity_if_needed() const {
    // Check if the session has timed out (away from creative app for too long)
    const auto now = std::chrono::steady_clock::now();
    if (session_start_ != 0
        && last_creative_seen_ != std::chrono::steady_clock::time_point{}
        && (now - last_creative_seen_) > kSessionTimeout) {
      // Session expired — reset so next creative focus starts a fresh timer
      session_start_ = 0;
      last_creative_app_key_.clear();
      rpc::log::info("Creative session expired after {}s away",
                     std::chrono::duration_cast<std::chrono::seconds>(now - last_creative_seen_).count());
    }

    if (!activity_is_set_) {
      return;
    }

    if (!ensure_discord_connected()) {
      return;
    }

    if (discord_.clear_activity()) {
      rpc::log::info("Discord activity cleared; active app is outside creative focus");
      last_activity_key_.clear();
      activity_is_set_ = false;
      // NOTE: we do NOT reset session_start_ here — it persists for quick returns
      return;
    }

    rpc::log::warn("Discord activity clear failed: {}", discord_.last_error());
    next_connect_attempt_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  }

  void log_creative_focus_miss(const rpc::ActiveWindowInfo& snapshot) const {
    const std::string process_name = snapshot.process_name.empty() ? "unknown" : snapshot.process_name;
    const std::string log_key = "miss\n" + process_name;
    if (log_key == last_focus_log_key_) {
      return;
    }

    last_focus_log_key_ = log_key;
    rpc::log::info("Creative focus ignored: {} is not a tracked creative app", process_name);
  }

  void log_creative_focus_match(const rpc::ActiveWindowInfo& snapshot,
                                const rpc::ActivityPayload& activity) const {
    const std::string process_name = snapshot.process_name.empty() ? "unknown" : snapshot.process_name;
    const std::string log_key = "match\n" + process_name + "\n" + activity.details + "\n" + activity.state;
    if (log_key == last_focus_log_key_) {
      return;
    }

    last_focus_log_key_ = log_key;
    rpc::log::info("Creative focus matched: {} -> {} / {}", process_name, activity.details, activity.state);
  }
};

} // namespace rpc::core
