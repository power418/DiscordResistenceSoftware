module;

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <rpc/core/export.hpp>

module rpc.core;

namespace rpc {

// Helper internal yang tidak perlu diexport
namespace {
    [[nodiscard]] std::string trim_internal(std::string_view value) {
      const auto first = value.find_first_not_of(" \t\r\n");
      if (first == std::string_view::npos) return {};
      const auto last = value.find_last_not_of(" \t\r\n");
      return std::string(value.substr(first, last - first + 1));
    }

    [[nodiscard]] std::string unquote_internal(std::string value) {
      if (value.size() < 2) return value;
      const char first = value.front();
      const char last = value.back();
      if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
        return value.substr(1, value.size() - 2);
      }
      return value;
    }
}

bool load_dotenv(const std::filesystem::path& path) {
  const auto resolved_path = find_dotenv_path(path);
  if (!resolved_path.has_value()) {
    return false;
  }

  std::ifstream file(*resolved_path);
  if (!file.is_open()) {
    return false;
  }

  std::string line;
  while (std::getline(file, line)) {
    const std::string clean_line = trim_internal(line);
    if (clean_line.empty() || clean_line[0] == '#') {
      continue;
    }

    auto pos = clean_line.find('=');
    if (pos == std::string::npos) {
      continue;
    }

    std::string key = trim_internal(std::string_view(clean_line).substr(0, pos));
    std::string value = unquote_internal(trim_internal(std::string_view(clean_line).substr(pos + 1)));
    if (key.empty()) {
      continue;
    }

    #if defined(_WIN32)
      _putenv_s(key.c_str(), value.c_str());
    #else
      setenv(key.c_str(), value.c_str(), 0);
    #endif
  }

  loaded_dotenv_path() = *resolved_path;
  std::error_code error;
  loaded_dotenv_write_time() = std::filesystem::last_write_time(*resolved_path, error);
  dotenv_has_loaded_once() = true;
  return true;
}

} // namespace rpc
