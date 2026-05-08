module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

export module rpc.config;

export namespace rpc {

[[nodiscard]] inline std::filesystem::path settings_path() {
  return std::filesystem::current_path() / "config.json";
}

struct Config {
  bool autostart = false;
  bool generic_mode = true;
  bool show_file_name = false;
  std::uint32_t poll_interval_ms = 1000;
  std::uint32_t debounce_ms = 1500;
  std::string log_level = "info";
};

namespace config {

namespace fonts {
extern const std::string_view heading_family;
extern const std::string_view body_family;
extern const std::string_view icon_family;
} // namespace fonts

namespace url {
extern const std::string_view google_fonts_preconnect;
extern const std::string_view google_fonts_static_preconnect;
extern const std::string_view google_fonts_link;
extern const std::string_view font_awesome_link;
} // namespace url

namespace win32 {
// Shared Win32 shell labels and manifest settings.
extern const std::wstring_view app_name;
extern const std::wstring_view window_class_name;
extern const std::wstring_view tray_tooltip;
extern const std::wstring_view splash_message;
extern const std::wstring_view recent_activity_title;
extern const std::string_view common_controls_manifest_dependency;
} // namespace win32

} // namespace config

[[nodiscard]] inline Config default_config() { return {}; }

[[nodiscard]] inline Config load_config_or_default(const std::filesystem::path& path) {
  Config cfg{};
  std::ifstream file(path, std::ios::in | std::ios::binary);
  if (!file) {
    return cfg;
  }

  try {
    nlohmann::json json;
    file >> json;

    if (json.contains("autostart")) {
      cfg.autostart = json.value("autostart", cfg.autostart);
    }
    if (json.contains("generic_mode")) {
      cfg.generic_mode = json.value("generic_mode", cfg.generic_mode);
    }
    if (json.contains("show_file_name")) {
      cfg.show_file_name = json.value("show_file_name", cfg.show_file_name);
    }
    if (json.contains("poll_interval_ms")) {
      cfg.poll_interval_ms = json.value("poll_interval_ms", cfg.poll_interval_ms);
    }
    if (json.contains("debounce_ms")) {
      cfg.debounce_ms = json.value("debounce_ms", cfg.debounce_ms);
    }
    if (json.contains("log_level")) {
      cfg.log_level = json.value("log_level", cfg.log_level);
    }
  } catch (...) {
    return default_config();
  }

  return cfg;
}

inline void save_config(const std::filesystem::path& path, const Config& cfg) {
  try {
    nlohmann::json json;
    json["autostart"] = cfg.autostart;
    json["generic_mode"] = cfg.generic_mode;
    json["show_file_name"] = cfg.show_file_name;
    json["poll_interval_ms"] = cfg.poll_interval_ms;
    json["debounce_ms"] = cfg.debounce_ms;
    json["log_level"] = cfg.log_level;

    std::ofstream file(path, std::ios::out | std::ios::binary);
    if (file) {
      file << json.dump(2);
    }
  } catch (...) {
    // Ignore save errors
  }
}

} // namespace rpc
