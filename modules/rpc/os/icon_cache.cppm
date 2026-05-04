module;

#include <cstdint>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <system_error>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <stb_image.h>
#include <nlohmann/json.hpp>

export module rpc.os.icon_cache;

import rpc.core;
import rpc.os.icon_extractor;
import rpc.net.imgur_uploader;
import rpc.utils.logger;

export namespace rpc {

class IconCache {
public:
  static constexpr std::uint32_t cache_schema_version = 4;

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

    // 3. Check whether a direct image URL can be uploaded
    const std::string imgur_client_id = rpc::env_or("IMGUR_CLIENT_ID", "");
    if (imgur_client_id.empty()) {
      rpc::log::debug("IMGUR_CLIENT_ID not set; using anonymous upload fallback");
    }

    // 4. Prefer bundled resource icons for known creative apps.
    if (auto resource_data = load_bundled_icon(exe_path); !resource_data.empty()) {
      rpc::log::info("Using bundled icon from res for: {}", exe_path);
      auto url = rpc::net::upload_to_imgur(resource_data, imgur_client_id);
      if (url.has_value() && !url->empty()) {
        memory_cache_[key] = *url;
        save_file_cache();
        rpc::log::info("Icon uploaded: {} -> {}", key, *url);
        return *url;
      }

      rpc::log::warn("Bundled icon upload failed for: {}", key);
    }

    // 5. Fallback to the active window first, then the exe icon.
    rpc::log::info("Extracting icon from: {}", exe_path);
    auto image_data = rpc::extract_icon_png(window_handle, exe_path);
    if (image_data.empty()) {
      rpc::log::warn("Icon extraction failed for: {}", exe_path);
      return {};
    }

    rpc::log::info("Icon extracted ({} bytes), uploading to public host...", image_data.size());

    // 6. Upload to a public host
    auto url = rpc::net::upload_to_imgur(image_data, imgur_client_id);
    if (!url.has_value() || url->empty()) {
      rpc::log::warn("Icon upload failed for: {}", key);
      return {};
    }

    rpc::log::info("Icon uploaded: {} -> {}", key, *url);

    // 7. Cache the result
    memory_cache_[key] = *url;
    save_file_cache();

    return *url;
  }

private:
  std::unordered_map<std::string, std::string> memory_cache_;
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

  [[nodiscard]] static std::filesystem::path resource_root() {
#ifdef SOFTWARE_RPC_RESOURCE_DIR
    return std::filesystem::path(SOFTWARE_RPC_RESOURCE_DIR);
#else
    return "res";
#endif
  }

  [[nodiscard]] static bool is_bundled_icon_app(std::string_view normalized_exe_path) {
    return normalized_exe_path.find("ableton") != std::string::npos ||
           normalized_exe_path.find("fl64") != std::string::npos ||
           normalized_exe_path.find("fl studio") != std::string::npos ||
           normalized_exe_path.find("image-line") != std::string::npos;
  }

  [[nodiscard]] static std::filesystem::path bundled_icon_path(std::string_view exe_path) {
    std::string normalized = cache_key(exe_path);
    if (normalized.empty() || !is_bundled_icon_app(normalized)) {
      return {};
    }

    if (normalized.find("ableton") != std::string::npos) {
      return resource_root() / "icon" / "ableton.png";
    }

    if (normalized.find("fl64") != std::string::npos ||
        normalized.find("fl studio") != std::string::npos ||
        normalized.find("image-line") != std::string::npos) {
      return resource_root() / "icon" / "fl-studio.webp";
    }

    return {};
  }

  [[nodiscard]] static std::vector<std::uint8_t> read_file_bytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
      return {};
    }

    file.seekg(0, std::ios::end);
    const auto size = file.tellg();
    if (size <= 0) {
      return {};
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file) {
      return {};
    }

    return bytes;
  }

  [[nodiscard]] static bool is_valid_image(const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty()) {
      return false;
    }

    int width = 0;
    int height = 0;
    int comp = 0;
    return stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                 &width, &height, &comp) != 0;
  }

  [[nodiscard]] static std::vector<std::uint8_t> load_bundled_icon(std::string_view exe_path) {
    const auto path = bundled_icon_path(exe_path);
    if (path.empty() || !std::filesystem::exists(path)) {
      return {};
    }

    auto bytes = read_file_bytes(path);
    if (!is_valid_image(bytes)) {
      rpc::log::warn("Bundled icon is not a valid image: {}", path.string());
      return {};
    }

    return bytes;
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
      if (!json.is_object() ||
          json.value("schema_version", 0U) != cache_schema_version) {
        rpc::log::info("Ignoring legacy icon cache at {}", path.string());
        return;
      }

      const auto entries_it = json.find("entries");
      if (entries_it == json.end() || !entries_it->is_object()) {
        return;
      }

      for (auto& [key, value] : entries_it->items()) {
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
    if (!path.parent_path().empty()) {
      std::error_code error;
      std::filesystem::create_directories(path.parent_path(), error);
    }

    nlohmann::json json = nlohmann::json::object();
    json["schema_version"] = cache_schema_version;
    json["entries"] = nlohmann::json::object();
    for (const auto& [key, url] : memory_cache_) {
      if (!url.empty()) {
        json["entries"][key] = url;
      }
    }

    std::ofstream file(path, std::ios::trunc);
    if (file.is_open()) {
      file << json.dump(2);
    }
  }
};

} // namespace rpc
