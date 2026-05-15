#pragma once

#include <cstdlib>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

#include <rpc/core/export.hpp>
#include <rpc/config/app.h>

namespace rpc {

[[nodiscard]] std::string trim(std::string_view value);
[[nodiscard]] std::string unquote(std::string value);

[[nodiscard]] inline std::optional<std::filesystem::path>
find_dotenv_path(const std::filesystem::path& requested_path = ".env") {
  if (std::filesystem::exists(requested_path)) {
    return std::filesystem::absolute(requested_path);
  }

  if (requested_path.has_parent_path()) {
    return std::nullopt;
  }

  std::error_code error;
  std::filesystem::path current = std::filesystem::current_path(error);
  if (error) {
    return std::nullopt;
  }

  while (!current.empty()) {
    const auto candidate = current / requested_path;
    if (std::filesystem::exists(candidate)) {
      return std::filesystem::absolute(candidate);
    }

    const auto parent = current.parent_path();
    if (parent == current) {
      break;
    }
    current = parent;
  }

  return std::nullopt;
}

inline std::filesystem::path& loaded_dotenv_path() {
  static std::filesystem::path path;
  return path;
}

inline std::filesystem::file_time_type& loaded_dotenv_write_time() {
  static std::filesystem::file_time_type write_time{};
  return write_time;
}

inline bool& dotenv_has_loaded_once() {
  static bool loaded = false;
  return loaded;
}

// ---------------------------------------------------------------------------
// .env loader — reads KEY=VALUE pairs from a .env file
// ---------------------------------------------------------------------------

RPC_CORE_API bool load_dotenv(const std::filesystem::path& path = ".env");

inline bool sync_dotenv_if_changed(const std::filesystem::path& path = ".env") {
  if (!dotenv_has_loaded_once()) {
    return load_dotenv(path);
  }

  const std::filesystem::path& resolved_path = loaded_dotenv_path();
  if (resolved_path.empty() || !std::filesystem::exists(resolved_path)) {
    return load_dotenv(path);
  }

  std::error_code error;
  const auto current_write_time = std::filesystem::last_write_time(resolved_path, error);
  if (error) {
    return false;
  }

  if (current_write_time == loaded_dotenv_write_time()) {
    return false;
  }

  return load_dotenv(resolved_path);
}

[[nodiscard]] inline std::filesystem::path dotenv_path() {
  if (loaded_dotenv_path().empty()) {
    load_dotenv();
  }
  return loaded_dotenv_path();
}

// ---------------------------------------------------------------------------
// Environment helpers
// ---------------------------------------------------------------------------

[[nodiscard]] inline std::string env_or(const char* key, std::string_view fallback) {
#if defined(NDEBUG)
  (void)key;
  return std::string(fallback);
#else
#if defined(_WIN32)
  char* value = nullptr;
  std::size_t value_size = 0;
  if (_dupenv_s(&value, &value_size, key) == 0 && value != nullptr && value[0] != '\0') {
    std::string result(value);
    std::free(value);
    return result;
  }

  if (value != nullptr) {
    std::free(value);
  }
#else
  const char* val = std::getenv(key);
  if (val && val[0] != '\0') {
    return std::string(val);
  }
#endif
  return std::string(fallback);
#endif
}

// ---------------------------------------------------------------------------
// Application constants (read from environment / .env)
// ---------------------------------------------------------------------------

[[nodiscard]] inline std::string client_id() {
  return env_or("DISCORD_CLIENT_ID", rpc::config::kDefaultDiscordClientId);
}

[[nodiscard]] inline std::string app_name() {
  return env_or("APP_NAME", rpc::config::kDefaultAppName);
}

[[nodiscard]] inline std::string app_key() {
  return env_or("APP_KEY", rpc::config::kDefaultAppKey);
}

[[nodiscard]] inline std::string imgur_client_id() {
  return env_or("IMGUR_CLIENT_ID", rpc::config::kDefaultImgurClientId);
}



// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

inline std::string make_activity_line(std::string_view details, std::string_view state) {
  std::string line;
  line.reserve(details.size() + 3 + state.size());
  line.append(details);
  line.append(" - ");
  line.append(state);
  return line;
}

} // namespace rpc
