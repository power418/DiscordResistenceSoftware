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

struct Config {
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

} // namespace rpc
