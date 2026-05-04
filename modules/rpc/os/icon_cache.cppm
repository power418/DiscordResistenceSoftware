module;

#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <nlohmann/json.hpp>

export module rpc.os.icon_cache;

import rpc.core;
import rpc.os.icon_extractor;
import rpc.net.imgur_uploader;
import rpc.utils.logger;

export namespace rpc {

class IconCache {
public:
  /// Try to get a cached icon URL for the given exe, or extract + upload it.
  /// Returns the public image URL, or empty string on failure.
  [[nodiscard]] std::string resolve_icon_url(std::string_view exe_path) {
    if (exe_path.empty()) return {};

    // Derive a cache key from the exe filename (lowercase)
    const std::string key = cache_key(exe_path);
    if (key.empty()) return {};

    // 1. Check memory cache
    auto it = memory_cache_.find(key);
    if (it != memory_cache_.end()) {
      return it->second;
    }

    // 2. Check file cache
    ensure_file_cache_loaded();
    it = memory_cache_.find(key);
    if (it != memory_cache_.end()) {
      return it->second;
    }

    // 3. Check if Imgur is configured
    const std::string imgur_client_id = rpc::env_or("IMGUR_CLIENT_ID", "");
    if (imgur_client_id.empty()) {
      rpc::log::debug("IMGUR_CLIENT_ID not set; icon auto-upload disabled");
      return {};
    }

    // 4. Extract icon from exe
    rpc::log::info("Extracting icon from: {}", exe_path);
    auto png_data = rpc::extract_icon_png(exe_path);
    if (png_data.empty()) {
      rpc::log::warn("Icon extraction failed for: {}", exe_path);
      // Cache empty string to avoid retrying
      memory_cache_[key] = "";
      return {};
    }

    rpc::log::info("Icon extracted ({} bytes), uploading to Imgur...", png_data.size());

    // 5. Upload to Imgur
    auto url = rpc::net::upload_to_imgur(png_data, imgur_client_id);
    if (!url.has_value() || url->empty()) {
      rpc::log::warn("Imgur upload failed for: {}", key);
      memory_cache_[key] = "";
      return {};
    }

    rpc::log::info("Icon uploaded: {} → {}", key, *url);

    // 6. Cache the result
    memory_cache_[key] = *url;
    save_file_cache();

    return *url;
  }

private:
  std::unordered_map<std::string, std::string> memory_cache_;
  bool file_cache_loaded_ = false;

  [[nodiscard]] static std::string cache_key(std::string_view exe_path) {
    std::string filename = std::filesystem::path(exe_path).filename().string();
    // Lowercase
    for (auto& c : filename) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return filename;
  }

  [[nodiscard]] static std::filesystem::path cache_file_path() {
    // Store next to the .env file
    auto env_path = rpc::dotenv_path();
    if (env_path.empty()) {
      return "icon_cache.json";
    }
    return env_path.parent_path() / "icon_cache.json";
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
      for (auto& [key, value] : json.items()) {
        if (value.is_string()) {
          memory_cache_[key] = value.get<std::string>();
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
    nlohmann::json json;
    for (const auto& [key, url] : memory_cache_) {
      if (!url.empty()) {
        json[key] = url;
      }
    }

    std::ofstream file(path, std::ios::trunc);
    if (file.is_open()) {
      file << json.dump(2);
    }
  }
};

} // namespace rpc
