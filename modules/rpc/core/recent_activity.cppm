module;

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

export module rpc.core.recent_activity;

import rpc.activity;
import rpc.core;
import rpc.os.active_window;
import rpc.utils.logger;

export namespace rpc {

struct RecentActivityEntry {
  std::string display_name;
  std::string details;
  std::string state;
  std::string process_name;
  std::string exe_path;
  std::string window_title;
  std::uint64_t first_seen_unix = 0;
  std::uint64_t last_seen_unix = 0;
  std::uint32_t seen_count = 0;
  bool supported = false;
};

class RecentActivityStore {
public:
  static constexpr std::uint32_t cache_schema_version = 1;

  [[nodiscard]] static std::string history_key(const ActiveWindowInfo& snapshot) {
    if (!snapshot.exe_path.empty()) {
      std::string key = normalize_key(snapshot.exe_path);
      if (!key.empty()) {
        return key;
      }
    }

    if (!snapshot.process_name.empty()) {
      std::string key = normalize_key(snapshot.process_name);
      if (!key.empty()) {
        return key;
      }
    }

    if (!snapshot.title.empty()) {
      return normalize_key(snapshot.title);
    }

    return {};
  }

  void record(const ActiveWindowInfo& snapshot,
              const std::optional<ActivityPayload>& activity,
              bool new_visit) {
    const std::string key = history_key(snapshot);
    if (key.empty()) {
      return;
    }

    ensure_file_cache_loaded();

    const std::uint64_t now = current_unix_timestamp_seconds();
    auto& entry = memory_cache_[key];
    const bool is_new_entry = entry.first_seen_unix == 0ULL;
    const bool supported = activity.has_value();
    const std::string display_name = determine_display_name(snapshot, activity);
    const std::string details = determine_details(snapshot, activity);
    const std::string state = determine_state(snapshot, activity, display_name);

    if (is_new_entry) {
      entry.first_seen_unix = now;
      entry.seen_count = 1U;
      new_visit = true;
    } else if (new_visit) {
      ++entry.seen_count;
    }

    entry.last_seen_unix = now;
    entry.display_name = display_name;
    entry.details = details;
    entry.state = state;
    entry.process_name = snapshot.process_name;
    entry.exe_path = snapshot.exe_path;
    entry.window_title = snapshot.title;
    entry.supported = supported;

    prune_expired(now);

    const bool should_persist = new_visit || is_new_entry || now >= last_persist_unix_ + persist_interval_seconds();
    if (should_persist) {
      save_file_cache();
      last_persist_unix_ = now;
    }
  }

  [[nodiscard]] std::string summary(std::size_t limit = 10) const {
    ensure_file_cache_loaded();
    prune_expired(current_unix_timestamp_seconds());

    std::vector<const RecentActivityEntry*> entries;
    entries.reserve(memory_cache_.size());
    for (const auto& [key, entry] : memory_cache_) {
      (void)key;
      if (!entry.display_name.empty() || !entry.process_name.empty() || !entry.exe_path.empty()) {
        entries.push_back(&entry);
      }
    }

    if (entries.empty()) {
      return {};
    }

    std::sort(entries.begin(), entries.end(), [](const RecentActivityEntry* left,
                                                 const RecentActivityEntry* right) {
      if (left->last_seen_unix != right->last_seen_unix) {
        return left->last_seen_unix > right->last_seen_unix;
      }
      if (left->seen_count != right->seen_count) {
        return left->seen_count > right->seen_count;
      }
      return left->display_name < right->display_name;
    });

    const std::uint64_t now = current_unix_timestamp_seconds();
    std::string output;
    output.reserve(entries.size() * 128U);
    output += "Recent activity (last 30 days)\n";

    const std::size_t visible_count = std::min(limit, entries.size());
    for (std::size_t index = 0; index < visible_count; ++index) {
      const auto& entry = *entries[index];
      output += std::to_string(index + 1);
      output += ". ";
      output += entry.display_name.empty() ? fallback_display_name(entry) : entry.display_name;
      output += "\n";

      const std::string subtitle = build_subtitle(entry);
      if (!subtitle.empty()) {
        output += "   ";
        output += subtitle;
        output += "\n";
      }

      output += "   Last seen: ";
      output += format_relative_time(now, entry.last_seen_unix);
      output += " | Sessions: ";
      output += std::to_string(entry.seen_count);
      output += "\n";

      if (index + 1 < visible_count) {
        output += "\n";
      }
    }

    if (entries.size() > visible_count) {
      output += "\n...";
      output += " and ";
      output += std::to_string(entries.size() - visible_count);
      output += " more\n";
    }

    return output;
  }

  void save_file_cache() const {
    const auto path = cache_file_path();
    if (!path.parent_path().empty()) {
      std::error_code error;
      std::filesystem::create_directories(path.parent_path(), error);
    }

    nlohmann::json json = nlohmann::json::object();
    json["schema_version"] = cache_schema_version;
    json["entries"] = nlohmann::json::object();

    for (const auto& [key, record] : memory_cache_) {
      if (record.last_seen_unix == 0ULL) {
        continue;
      }

      json["entries"][key] = {
        {"display_name", record.display_name},
        {"details", record.details},
        {"state", record.state},
        {"process_name", record.process_name},
        {"exe_path", record.exe_path},
        {"window_title", record.window_title},
        {"first_seen_unix", record.first_seen_unix},
        {"last_seen_unix", record.last_seen_unix},
        {"seen_count", record.seen_count},
        {"supported", record.supported},
      };
    }

    std::ofstream file(path, std::ios::trunc);
    if (file.is_open()) {
      file << json.dump(2);
    }
  }

private:
  struct CacheRecord : RecentActivityEntry {};

  mutable std::unordered_map<std::string, CacheRecord> memory_cache_;
  mutable bool file_cache_loaded_ = false;
  mutable std::uint64_t last_persist_unix_ = 0;

  [[nodiscard]] static std::string normalize_key(std::string_view value) {
    std::string normalized = std::filesystem::path(value).lexically_normal().generic_string();
    if (normalized.empty()) {
      return {};
    }

    for (auto& character : normalized) {
      character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return normalized;
  }

  [[nodiscard]] static std::string fallback_display_name(const RecentActivityEntry& entry) {
    if (!entry.exe_path.empty()) {
      const std::filesystem::path exe_path(entry.exe_path);
      if (!exe_path.stem().empty()) {
        return exe_path.stem().string();
      }
    }

    if (!entry.process_name.empty()) {
      const std::filesystem::path process_name(entry.process_name);
      if (!process_name.stem().empty()) {
        return process_name.stem().string();
      }
      return entry.process_name;
    }

    if (!entry.window_title.empty()) {
      return entry.window_title;
    }

    return "Unknown app";
  }

  [[nodiscard]] static std::string determine_display_name(
    const ActiveWindowInfo& snapshot,
    const std::optional<ActivityPayload>& activity) {
    if (activity.has_value() && !activity->state.empty()) {
      return activity->state;
    }

    return fallback_display_name_from_snapshot(snapshot);
  }

  [[nodiscard]] static std::string determine_details(const ActiveWindowInfo& snapshot,
                                                     const std::optional<ActivityPayload>& activity) {
    if (activity.has_value() && !activity->details.empty()) {
      return activity->details;
    }

    if (!snapshot.title.empty()) {
      return snapshot.title;
    }

    return {};
  }

  [[nodiscard]] static std::string determine_state(const ActiveWindowInfo& snapshot,
                                                   const std::optional<ActivityPayload>& activity,
                                                   const std::string& display_name) {
    if (activity.has_value() && !activity->state.empty()) {
      return activity->state;
    }

    if (!display_name.empty()) {
      return display_name;
    }

    return fallback_display_name_from_snapshot(snapshot);
  }

  [[nodiscard]] static std::string fallback_display_name_from_snapshot(const ActiveWindowInfo& snapshot) {
    if (!snapshot.exe_path.empty()) {
      const std::filesystem::path exe_path(snapshot.exe_path);
      if (!exe_path.stem().empty()) {
        return exe_path.stem().string();
      }
    }

    if (!snapshot.process_name.empty()) {
      const std::filesystem::path process_name(snapshot.process_name);
      if (!process_name.stem().empty()) {
        return process_name.stem().string();
      }
      return snapshot.process_name;
    }

    if (!snapshot.title.empty()) {
      return snapshot.title;
    }

    return "Unknown app";
  }

  [[nodiscard]] static std::string build_subtitle(const RecentActivityEntry& entry) {
    if (!entry.details.empty() && entry.details != entry.display_name) {
      return entry.details;
    }

    if (!entry.window_title.empty() && entry.window_title != entry.display_name) {
      return entry.window_title;
    }

    if (!entry.process_name.empty() && entry.process_name != entry.display_name) {
      return entry.process_name;
    }

    return {};
  }

  [[nodiscard]] static std::string format_relative_time(std::uint64_t now,
                                                        std::uint64_t past) {
    if (now <= past) {
      return "just now";
    }

    const std::uint64_t delta = now - past;
    if (delta < 60ULL) {
      return std::to_string(delta) + "s ago";
    }

    const std::uint64_t minutes = delta / 60ULL;
    if (minutes < 60ULL) {
      return std::to_string(minutes) + "m ago";
    }

    const std::uint64_t hours = minutes / 60ULL;
    if (hours < 24ULL) {
      return std::to_string(hours) + "h ago";
    }

    const std::uint64_t days = hours / 24ULL;
    return std::to_string(days) + "d ago";
  }

  [[nodiscard]] static std::uint64_t current_unix_timestamp_seconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
      duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
  }

  [[nodiscard]] static std::uint64_t cache_ttl_seconds() {
    return 30ULL * 24ULL * 60ULL * 60ULL;
  }

  [[nodiscard]] static std::uint64_t persist_interval_seconds() {
    return 60ULL;
  }

  [[nodiscard]] static std::filesystem::path cache_file_path() {
    const auto env_path = rpc::dotenv_path();
    if (!env_path.empty()) {
      return env_path.parent_path() / "recent_activity.json";
    }

#if defined(_WIN32)
    const std::string local_app_data = rpc::env_or("LOCALAPPDATA", "");
    if (!local_app_data.empty()) {
      return std::filesystem::path(local_app_data) / rpc::app_name() / "recent_activity.json";
    }

    const std::string app_data = rpc::env_or("APPDATA", "");
    if (!app_data.empty()) {
      return std::filesystem::path(app_data) / rpc::app_name() / "recent_activity.json";
    }
#else
    const std::string xdg_cache_home = rpc::env_or("XDG_CACHE_HOME", "");
    if (!xdg_cache_home.empty()) {
      return std::filesystem::path(xdg_cache_home) / rpc::app_name() / "recent_activity.json";
    }

    const std::string home = rpc::env_or("HOME", "");
    if (!home.empty()) {
      return std::filesystem::path(home) / ".cache" / rpc::app_name() / "recent_activity.json";
    }
#endif

    return "recent_activity.json";
  }

  void ensure_file_cache_loaded() const {
    if (file_cache_loaded_) {
      return;
    }

    file_cache_loaded_ = true;

    const auto path = cache_file_path();
    std::ifstream file(path);
    if (!file.is_open()) {
      return;
    }

    try {
      nlohmann::json json;
      file >> json;
      if (!json.is_object()) {
        return;
      }

      const std::uint32_t schema_version = json.value("schema_version", 0U);
      if (schema_version != cache_schema_version) {
        rpc::log::info("Ignoring legacy recent activity cache at {}", path.string());
        return;
      }

      const auto entries_it = json.find("entries");
      if (entries_it == json.end() || !entries_it->is_object()) {
        return;
      }

      for (const auto& [key, value] : entries_it->items()) {
        if (!value.is_object()) {
          continue;
        }

        CacheRecord record;
        record.display_name = value.value("display_name", std::string{});
        record.details = value.value("details", std::string{});
        record.state = value.value("state", std::string{});
        record.process_name = value.value("process_name", std::string{});
        record.exe_path = value.value("exe_path", std::string{});
        record.window_title = value.value("window_title", std::string{});
        record.first_seen_unix = value.value("first_seen_unix", 0ULL);
        record.last_seen_unix = value.value("last_seen_unix", 0ULL);
        record.seen_count = value.value("seen_count", 0U);
        record.supported = value.value("supported", false);

        if (record.display_name.empty()) {
          record.display_name = fallback_display_name(record);
        }
        if (record.state.empty()) {
          record.state = record.display_name;
        }

        if (record.first_seen_unix == 0ULL) {
          record.first_seen_unix = record.last_seen_unix;
        }

        if (record.last_seen_unix == 0ULL) {
          record.last_seen_unix = record.first_seen_unix;
        }

        if (record.seen_count == 0U) {
          record.seen_count = 1U;
        }

        if (record.last_seen_unix == 0ULL) {
          continue;
        }

        memory_cache_[key] = std::move(record);
      }

      prune_expired(current_unix_timestamp_seconds());
      rpc::log::info("Loaded {} recent activity entries from {}", memory_cache_.size(), path.string());
    } catch (...) {
      rpc::log::warn("Failed to parse recent activity file: {}", path.string());
    }
  }

  void prune_expired(std::uint64_t now) const {
    const std::uint64_t cutoff = now > cache_ttl_seconds() ? now - cache_ttl_seconds() : 0ULL;
    bool pruned = false;

    for (auto it = memory_cache_.begin(); it != memory_cache_.end();) {
      if (it->second.last_seen_unix != 0ULL && it->second.last_seen_unix < cutoff) {
        it = memory_cache_.erase(it);
        pruned = true;
      } else {
        ++it;
      }
    }

    if (pruned) {
      save_file_cache();
    }
  }
};

} // namespace rpc
