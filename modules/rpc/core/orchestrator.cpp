module;
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <optional>
#include <thread>

module rpc.core.orchestrator;

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

namespace rpc::core {

Orchestrator::~Orchestrator() {
    save_history();
}

void Orchestrator::poll_once() const {
    if (rpc::sync_dotenv_if_changed()) {
      rpc::log::info(".env synced: {}", rpc::dotenv_path().string());
    }

    const auto snapshot = rpc::query_active_window();
    if (!snapshot.process_name.empty() && snapshot.process_name != last_seen_process_) {
      last_seen_process_ = snapshot.process_name;
      rpc::log::debug("Active window: {}", snapshot.process_name);
    }

    auto detected = rpc::detectors::detect_creative_activity(snapshot);
    if (!detected.has_value()) {
      detected = rpc::detectors::detect_office_activity(snapshot);
    }

    std::optional<rpc::ActivityPayload> productive_detected;
    if (!detected.has_value()) {
      productive_detected = rpc::detectors::detect_productive_activity(snapshot);
    }

    const std::optional<rpc::ActivityPayload> activity_for_history =
      detected.has_value() ? detected : productive_detected;
    const std::string history_key = rpc::RecentActivityStore::history_key(snapshot);
    if (!history_key.empty()) {
      recent_activity_.record(snapshot, activity_for_history, history_key != last_history_key_);
      last_history_key_ = history_key;
    }

    if (detected.has_value()) {
      adopt_creative_app(snapshot, detected.value());
      return;
    }

    if (productive_detected.has_value()) {
      adopt_creative_app(snapshot, productive_detected.value());
      return;
    }

    if (sticky_process_.empty()) {
      sticky_process_ = "__idle__";
      sticky_exe_path_.clear();
      session_start_ = rpc::unix_timestamp_seconds_now();

      rpc::ActivityPayload idle_activity{};
      idle_activity.details = rpc::app_name().empty() ? std::string("software_discord_rpc") : rpc::app_name();
      idle_activity.state = snapshot.process_name.empty() ? std::string("Monitoring") : snapshot.process_name;
      idle_activity.start_timestamp_unix = session_start_;
      sticky_activity_ = idle_activity;
      send_activity(idle_activity.details + "\n" + idle_activity.state);
      return;
    }

    if (!sticky_process_.empty() && sticky_process_ != "__idle__") {
      if (rpc::is_process_running(sticky_process_)) {
        ensure_activity_sent();
        return;
      }

      rpc::log::info("{} has exited, clearing Discord activity", sticky_process_);
      clear_activity();
      sticky_process_.clear();
      sticky_exe_path_.clear();
      sticky_activity_ = {};
      session_start_ = 0;
      return;
    }

    if (sticky_process_ == "__idle__") {
      ensure_activity_sent();
      return;
    }
}

void Orchestrator::tick_once() const {
    poll_once();
    std::this_thread::sleep_for(poll_interval_);
}

std::string Orchestrator::recent_activity_summary(std::size_t limit) const {
    return recent_activity_.summary(limit);
}

void Orchestrator::save_history() const {
    recent_activity_.save_file_cache();
}

void Orchestrator::adopt_creative_app(const rpc::ActiveWindowInfo& snapshot,
                                      const rpc::ActivityPayload& detected) const {
    const std::string activity_key = detected.details + "\n" + detected.state;

    const bool is_different_app = (sticky_process_ != snapshot.process_name);
    const bool is_title_changed = (last_activity_key_ != activity_key);

    if (is_different_app) {
      rpc::log::info("Creative app detected: {} → {} / {}",
                     snapshot.process_name, detected.details, detected.state);
      sticky_process_ = snapshot.process_name;
      sticky_exe_path_ = snapshot.exe_path;
      session_start_ = rpc::unix_timestamp_seconds_now();
    } else if (is_title_changed && session_start_ == 0) {
      session_start_ = rpc::unix_timestamp_seconds_now();
    }

    if (is_title_changed) {
      rpc::log::info("RPC details: {} ({})", detected.details, detected.state);
    }

    sticky_activity_ = detected;
    sticky_activity_.start_timestamp_unix = session_start_;

    std::string icon_url = icon_cache_.resolve_icon_url(snapshot.window_handle, sticky_exe_path_);
    const std::string fallback_large_image = rpc::env_or("DISCORD_LARGE_IMAGE", rpc::config::kDefaultDiscordLargeImage);
    sticky_activity_.large_image = icon_url.empty() ? fallback_large_image : icon_url;
    sticky_activity_.large_text = rpc::env_or("DISCORD_LARGE_TEXT", 
                                             rpc::config::kDefaultDiscordLargeText.empty() ? detected.state : rpc::config::kDefaultDiscordLargeText);

    const bool is_large_image_changed = (sticky_activity_.large_image != last_large_image_);

    const bool need_update = is_different_app || is_title_changed
                          || is_large_image_changed
                          || !activity_is_set_ || !discord_.connected();
    if (need_update) {
      send_activity(activity_key);
    }
}

void Orchestrator::ensure_activity_sent() const {
    if (activity_is_set_ && discord_.connected()) {
      return;
    }

    if (sticky_activity_.details.empty()) {
      return;
    }

    const std::string key = sticky_activity_.details + "\n" + sticky_activity_.state;
    send_activity(key);
}

void Orchestrator::send_activity(const std::string& activity_key) const {
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

void Orchestrator::clear_activity() const {
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

bool Orchestrator::ensure_discord_connected() const {
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

} // namespace rpc::core
