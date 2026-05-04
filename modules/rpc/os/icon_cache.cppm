module;

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

#include <nlohmann/json.hpp>

export module rpc.os.icon_cache;

import rpc.core;
import rpc.os.icon_extractor;
import rpc.net.imgur_uploader;
import rpc.utils.logger;

export namespace rpc {

class IconCache {
public:
  static constexpr std::uint32_t cache_schema_version = 6;

  /// Try to get a cached icon URL for the given exe, or extract + upload it.
  /// Returns the public image URL, or empty string on failure.
  [[nodiscard]] std::string resolve_icon_url(std::string_view exe_path) {
    return resolve_icon_url(0, exe_path);
  }

  /// Try to get a cached icon URL for the current window/exe, or extract + upload it.
  /// Returns the public image URL, or empty string on failure.
  [[nodiscard]] std::string resolve_icon_url(std::uintptr_t window_handle,
                                             std::string_view exe_path) {
    if (exe_path.empty()) return {};

    // Derive a cache key from the normalized exe path (lowercase)
    const std::string key = cache_key(exe_path);
    if (key.empty()) return {};

    const std::uint64_t now = current_unix_timestamp_seconds();

    // 1. Check memory cache
    auto it = memory_cache_.find(key);
    if (it != memory_cache_.end()) {
      if (!it->second.url.empty()) {
        return it->second.url;
      }

      if (it->second.next_retry_unix > now) {
        return {};
      }
    }

    // 2. Check file cache
    ensure_file_cache_loaded();
    it = memory_cache_.find(key);
    if (it != memory_cache_.end()) {
      if (!it->second.url.empty()) {
        return it->second.url;
      }

      if (it->second.next_retry_unix > now) {
        return {};
      }
    }

    // 3. Check whether a direct image URL can be uploaded
    const std::string imgur_client_id = rpc::env_or("IMGUR_CLIENT_ID", "");
    static bool warned_anonymous_upload = false;
    if (imgur_client_id.empty() && !warned_anonymous_upload) {
      rpc::log::debug("IMGUR_CLIENT_ID not set; using anonymous upload fallback");
      warned_anonymous_upload = true;
    }

    // 4. Fallback to the active window first, then the exe icon.
    rpc::log::info("Extracting icon from: {}", exe_path);
    auto image_data = rpc::extract_icon_png(window_handle, exe_path);
    if (image_data.empty()) {
      rpc::log::warn("Icon extraction failed for: {}", exe_path);
      return {};
    }

    rpc::log::info("Icon extracted ({} bytes), uploading to public host...", image_data.size());

    // 5. Upload to a public host
    auto url = rpc::net::upload_to_imgur(image_data, imgur_client_id);
    if (!url.has_value() || url->empty()) {
      rpc::log::warn("Icon upload failed for: {}", key);
      record_upload_failure(key);
      return {};
    }

    rpc::log::info("Icon uploaded: {} -> {}", key, *url);

    // 7. Cache the result
    record_upload_success(key, *url);
    save_file_cache();

    return *url;
  }

private:
  struct CacheRecord {
    std::string url;
    std::uint64_t next_retry_unix = 0;
    std::uint32_t failure_count = 0;
  };

  std::unordered_map<std::string, CacheRecord> memory_cache_;
  bool file_cache_loaded_ = false;

  [[nodiscard]] static std::string cache_key(std::string_view exe_path) {
    std::string normalized = std::filesystem::path(exe_path).lexically_normal().generic_string();
    if (normalized.empty()) {
      return {};
    }

    // Lowercase
    for (auto& c : normalized) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return normalized;
  }


  [[nodiscard]] static std::uint64_t retry_delay_seconds(std::uint32_t failure_count) {
    constexpr std::uint64_t initial_delay_seconds = 10ULL * 60ULL;
    constexpr std::uint64_t maximum_delay_seconds = 24ULL * 60ULL * 60ULL;

    if (failure_count <= 1U) {
      return initial_delay_seconds;
    }

    std::uint64_t delay = initial_delay_seconds;
    const std::uint32_t doublings = std::min<std::uint32_t>(failure_count - 1U, 8U);
    for (std::uint32_t i = 0; i < doublings; ++i) {
      if (delay >= maximum_delay_seconds / 2ULL) {
        return maximum_delay_seconds;
      }
      delay *= 2ULL;
    }

    return std::min(delay, maximum_delay_seconds);
  }

  void record_upload_success(const std::string& key, const std::string& url) {
    auto& entry = memory_cache_[key];
    entry.url = url;
    entry.failure_count = 0;
    entry.next_retry_unix = 0;
  }

  void record_upload_failure(const std::string& key) {
    auto& entry = memory_cache_[key];
    entry.url.clear();
    entry.failure_count = entry.failure_count == 0 ? 1U : entry.failure_count + 1U;
    entry.next_retry_unix = current_unix_timestamp_seconds() + retry_delay_seconds(entry.failure_count);
    save_file_cache();
  }

  [[nodiscard]] static std::uint64_t current_unix_timestamp_seconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
      duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
  }

  [[nodiscard]] static std::filesystem::path cache_file_path() {
    // Prefer a project-local cache when a .env exists, otherwise use a stable
    // per-user cache location so the working directory does not affect caching.
    const auto env_path = rpc::dotenv_path();
    if (!env_path.empty()) {
      return env_path.parent_path() / "icon_cache.json";
    }

#if defined(_WIN32)
    const std::string local_app_data = rpc::env_or("LOCALAPPDATA", "");
    if (!local_app_data.empty()) {
      return std::filesystem::path(local_app_data) / rpc::app_name() / "icon_cache.json";
    }

    const std::string app_data = rpc::env_or("APPDATA", "");
    if (!app_data.empty()) {
      return std::filesystem::path(app_data) / rpc::app_name() / "icon_cache.json";
    }
#else
    const std::string xdg_cache_home = rpc::env_or("XDG_CACHE_HOME", "");
    if (!xdg_cache_home.empty()) {
      return std::filesystem::path(xdg_cache_home) / rpc::app_name() / "icon_cache.json";
    }

    const std::string home = rpc::env_or("HOME", "");
    if (!home.empty()) {
      return std::filesystem::path(home) / ".cache" / rpc::app_name() / "icon_cache.json";
    }
#endif

    return "icon_cache.json";
  }

  void ensure_file_cache_loaded() {
    if (file_cache_loaded_) return;
    file_cache_loaded_ = true;

    const auto path = cache_file_path();
    std::ifstream file(path);
    if (!file.is_open()) return;

    try {
      nlohmann::json json;
      file >> json;
      if (!json.is_object()) {
        return;
      }

      const std::uint32_t schema_version = json.value("schema_version", 0U);
      if (schema_version != cache_schema_version) {
        rpc::log::info("Ignoring legacy icon cache at {}", path.string());
        return;
      }

      const auto entries_it = json.find("entries");
      if (entries_it == json.end() || !entries_it->is_object()) {
        return;
      }

      for (auto& [key, value] : entries_it->items()) {
        CacheRecord record;
        if (value.is_string()) {
          record.url = value.get<std::string>();
        } else if (value.is_object()) {
          if (const auto url_it = value.find("url"); url_it != value.end() && url_it->is_string()) {
            record.url = url_it->get<std::string>();
          }

          if (const auto failure_it = value.find("failure_count");
              failure_it != value.end() && failure_it->is_number_unsigned()) {
            record.failure_count = failure_it->get<std::uint32_t>();
          } else if (const auto failure_it = value.find("failure_count");
                     failure_it != value.end() && failure_it->is_number_integer()) {
            const auto failure_count = failure_it->get<std::int64_t>();
            record.failure_count = failure_count > 0
                                 ? static_cast<std::uint32_t>(failure_count)
                                 : 0U;
          }

          if (const auto retry_it = value.find("next_retry_unix");
              retry_it != value.end() && retry_it->is_number_unsigned()) {
            record.next_retry_unix = retry_it->get<std::uint64_t>();
          } else if (const auto retry_it = value.find("next_retry_unix");
                     retry_it != value.end() && retry_it->is_number_integer()) {
            const auto next_retry_unix = retry_it->get<std::int64_t>();
            record.next_retry_unix = next_retry_unix > 0
                                   ? static_cast<std::uint64_t>(next_retry_unix)
                                   : 0ULL;
          }
        }

        if (!record.url.empty() || record.failure_count > 0U || record.next_retry_unix > 0ULL) {
          memory_cache_[key] = std::move(record);
        }
      }
      rpc::log::info("Loaded {} cached icon URLs from {}", memory_cache_.size(),
                     path.string());
    } catch (...) {
      rpc::log::warn("Failed to parse icon cache file: {}", path.string());
    }
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
      if (!record.url.empty() || record.failure_count > 0U || record.next_retry_unix > 0ULL) {
        json["entries"][key] = {
          {"url", record.url},
          {"failure_count", record.failure_count},
          {"next_retry_unix", record.next_retry_unix},
        };
      }
    }

    std::ofstream file(path, std::ios::trunc);
    if (file.is_open()) {
      file << json.dump(2);
    }
  }
};

} // namespace rpc
