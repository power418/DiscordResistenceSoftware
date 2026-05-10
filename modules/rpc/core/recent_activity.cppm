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

  [[nodiscard]] static std::string history_key(const ActiveWindowInfo& snapshot);

  void record(const ActiveWindowInfo& snapshot,
              const std::optional<ActivityPayload>& activity,
              bool new_visit);

  [[nodiscard]] std::string summary(std::size_t limit = 10) const;

  void save_file_cache() const;

private:
  struct CacheRecord : RecentActivityEntry {};

  mutable std::unordered_map<std::string, CacheRecord> memory_cache_;
  mutable bool file_cache_loaded_ = false;
  mutable std::uint64_t last_persist_unix_ = 0;

  [[nodiscard]] static std::string normalize_key(std::string_view value);
  [[nodiscard]] static std::string fallback_display_name(const RecentActivityEntry& entry);

  [[nodiscard]] static std::string determine_display_name(
    const ActiveWindowInfo& snapshot,
    const std::optional<ActivityPayload>& activity);

  [[nodiscard]] static std::string determine_details(const ActiveWindowInfo& snapshot,
                                                     const std::optional<ActivityPayload>& activity);

  [[nodiscard]] static std::string determine_state(const ActiveWindowInfo& snapshot,
                                                   const std::optional<ActivityPayload>& activity,
                                                   const std::string& display_name);

  [[nodiscard]] static std::string fallback_display_name_from_snapshot(const ActiveWindowInfo& snapshot);

  [[nodiscard]] static std::string build_subtitle(const RecentActivityEntry& entry);

  [[nodiscard]] static std::string format_relative_time(std::uint64_t now,
                                                        std::uint64_t past);

  [[nodiscard]] static std::uint64_t current_unix_timestamp_seconds();
  [[nodiscard]] static std::uint64_t cache_ttl_seconds();
  [[nodiscard]] static std::uint64_t persist_interval_seconds();
  [[nodiscard]] static std::filesystem::path cache_file_path();

  void ensure_file_cache_loaded() const;
  void prune_expired(std::uint64_t now) const;
};

} // namespace rpc
