#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(__has_include)
#  if __has_include(<source_location>)
#    include <source_location>
#  endif
#endif

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <modules/rpc/utils/logger.cppm>

namespace rpc::log {

[[nodiscard]] spdlog::level::level_enum to_spdlog(Level level) noexcept {
  switch (level) {
    case Level::trace:    return spdlog::level::trace;
    case Level::debug:    return spdlog::level::debug;
    case Level::info:     return spdlog::level::info;
    case Level::warn:     return spdlog::level::warn;
    case Level::error:    return spdlog::level::err;
    case Level::critical: return spdlog::level::critical;
    case Level::off:      return spdlog::level::off;
  }
  return spdlog::level::info;
}

namespace detail {
    void do_log(Level level, std::string_view message) {
        spdlog::log(to_spdlog(level), message);
    }
}

void init(std::string_view logger_name, Level level) {
  auto existing = spdlog::get(std::string(logger_name));
  if (existing) {
    spdlog::set_default_logger(existing);
    spdlog::set_level(to_spdlog(level));
    return;
  }

  std::vector<spdlog::sink_ptr> sinks;
  sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

  try {
    const char* temp_dir = std::getenv("TEMP");
    if (temp_dir == nullptr || temp_dir[0] == '\0') {
      temp_dir = std::getenv("TMP");
    }

    std::filesystem::path log_path = (temp_dir && temp_dir[0] != '\0')
      ? (std::filesystem::path(temp_dir) / "software_discord_rpc.log")
      : std::filesystem::path("software_discord_rpc.log");

    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), true));
  } catch (...) {
  }

  auto logger = std::make_shared<spdlog::logger>(std::string(logger_name), sinks.begin(), sinks.end());
  spdlog::register_logger(logger);
  spdlog::set_default_logger(logger);
  spdlog::set_level(to_spdlog(level));
  spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
}

void set_level(Level level) {
    spdlog::set_level(to_spdlog(level));
}

#if defined(RPC_LOG_HAS_SOURCE_LOCATION)
void info_at(std::string_view message, const std::source_location loc) {
    spdlog::info("[{}:{}] {}", loc.file_name(), loc.line(), message);
}

void warn_at(std::string_view message, const std::source_location loc) {
    spdlog::warn("[{}:{}] {}", loc.file_name(), loc.line(), message);
}

void error_at(std::string_view message, const std::source_location loc) {
    spdlog::error("[{}:{}] {}", loc.file_name(), loc.line(), message);
}
#else
void info_at(std::string_view message) {
    spdlog::info("{}", message);
}

void warn_at(std::string_view message) {
    spdlog::warn("{}", message);
}

void error_at(std::string_view message) {
    spdlog::error("{}", message);
}
#endif

} // namespace rpc::log
