#pragma once

#include <chrono>
#include <cstdint>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <utility>
#include <system_error>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <modules/rpc/core.cppm>
#include <modules/rpc/config.cppm>
#include <modules/rpc/os/icon_extractor.cppm>
#include <modules/rpc/net/imgur_uploader.cppm>
#include <modules/rpc/utils/logger.cppm>

namespace rpc {

class IconCache {
public:
  static constexpr std::uint32_t cache_schema_version = 6;

  [[nodiscard]] std::string resolve_icon_url(std::string_view exe_path) {
    return resolve_icon_url(0, exe_path);
  }

  [[nodiscard]] std::string resolve_icon_url(std::uintptr_t window_handle,
                                             std::string_view exe_path);

private:
  struct CacheRecord {
    std::string url;
    std::uint64_t next_retry_unix = 0;
    std::uint32_t failure_count = 0;
  };

  std::unordered_map<std::string, CacheRecord> memory_cache_;
  bool file_cache_loaded_ = false;

  [[nodiscard]] static std::string cache_key(std::string_view exe_path);
  [[nodiscard]] static std::uint64_t retry_delay_seconds(std::uint32_t failure_count);

  void record_upload_success(const std::string& key, const std::string& url);
  void record_upload_failure(const std::string& key);

  [[nodiscard]] static std::uint64_t current_unix_timestamp_seconds();
  [[nodiscard]] static std::filesystem::path cache_file_path();

  void ensure_file_cache_loaded();
  void save_file_cache() const;
};

} // namespace rpc
