#pragma once

#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#if defined(__has_include)
#  if __has_include(<source_location>)
#    include <source_location>
#    define RPC_LOG_HAS_SOURCE_LOCATION 1
#  endif
#endif

#include <fmt/format.h>

namespace rpc::log {

enum class Level : std::uint8_t {
  trace,
  debug,
  info,
  warn,
  error,
  critical,
  off,
};

template <typename T>
concept Loggable = requires(T&& v) {
  { fmt::format("{}", std::forward<T>(v)) } -> std::convertible_to<std::string>;
};

[[nodiscard]] constexpr Level build_default_level() noexcept {
#if defined(NDEBUG)
  return Level::info;
#else
  return Level::debug;
#endif
}

namespace detail {
    void do_log(Level level, std::string_view message);
}

void init(std::string_view logger_name = "software_discord_rpc",
                 Level level = build_default_level());

void set_level(Level level);

template <Level Lvl, typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void log(fmt::format_string<Args...> fmt_str, Args&&... args) {
  if constexpr (static_cast<std::uint8_t>(Lvl) >=
                static_cast<std::uint8_t>(build_default_level())) {
    detail::do_log(Lvl, fmt::format(fmt_str, std::forward<Args>(args)...));
  }
}

template <typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void trace(fmt::format_string<Args...> fmt_str, Args&&... args) {
  log<Level::trace>(fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void debug(fmt::format_string<Args...> fmt_str, Args&&... args) {
  log<Level::debug>(fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void info(fmt::format_string<Args...> fmt_str, Args&&... args) {
  log<Level::info>(fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void warn(fmt::format_string<Args...> fmt_str, Args&&... args) {
  log<Level::warn>(fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void error(fmt::format_string<Args...> fmt_str, Args&&... args) {
  log<Level::error>(fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void critical(fmt::format_string<Args...> fmt_str, Args&&... args) {
  log<Level::critical>(fmt_str, std::forward<Args>(args)...);
}

#if defined(RPC_LOG_HAS_SOURCE_LOCATION)
void info_at(std::string_view message,
             const std::source_location loc = std::source_location::current());

void warn_at(std::string_view message,
             const std::source_location loc = std::source_location::current());

void error_at(std::string_view message,
              const std::source_location loc = std::source_location::current());
#else
void info_at(std::string_view message);
void warn_at(std::string_view message);
void error_at(std::string_view message);
#endif

} // namespace rpc::log
