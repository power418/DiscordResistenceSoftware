module;

#include <concepts>
#include <cstdint>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

export module rpc.utils.logger;

export namespace rpc::log {

// ---------------------------------------------------------------------------
// Log level enum
// ---------------------------------------------------------------------------

enum class Level : std::uint8_t {
  trace,
  debug,
  info,
  warn,
  error,
  critical,
  off,
};

// ---------------------------------------------------------------------------
// Concept: anything spdlog can format via fmt  (string, arithmetic, etc.)
// ---------------------------------------------------------------------------

template <typename T>
concept Loggable = requires(T&& v) {
  { fmt::format("{}", std::forward<T>(v)) } -> std::convertible_to<std::string>;
};

// ---------------------------------------------------------------------------
// Level mapping
// ---------------------------------------------------------------------------

[[nodiscard]] constexpr spdlog::level::level_enum to_spdlog(Level level) noexcept {
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

[[nodiscard]] constexpr std::string_view level_name(Level level) noexcept {
  switch (level) {
    case Level::trace:    return "trace";
    case Level::debug:    return "debug";
    case Level::info:     return "info";
    case Level::warn:     return "warn";
    case Level::error:    return "error";
    case Level::critical: return "critical";
    case Level::off:      return "off";
  }
  return "unknown";
}

// ---------------------------------------------------------------------------
// Initialization & configuration
// ---------------------------------------------------------------------------

inline void init(std::string_view logger_name = "software_discord_rpc",
                 Level level = Level::info) {
  auto existing = spdlog::get(std::string(logger_name));
  if (existing) {
    spdlog::set_default_logger(existing);
    spdlog::set_level(to_spdlog(level));
    return;
  }

  auto logger = spdlog::stdout_color_mt(std::string(logger_name));
  spdlog::set_default_logger(logger);
  spdlog::set_level(to_spdlog(level));
  spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
}

inline void set_level(Level level) { spdlog::set_level(to_spdlog(level)); }

// ---------------------------------------------------------------------------
// Core generic log — variadic, level as template parameter
// ---------------------------------------------------------------------------

/// Generic log function: compile-time level + fmt-style format string.
///
///   rpc::log::log<Level::info>("Hello {} from port {}", name, port);
///
template <Level Lvl, typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void log(fmt::format_string<Args...> fmt_str, Args&&... args) {
  spdlog::log(to_spdlog(Lvl), fmt_str, std::forward<Args>(args)...);
}

// ---------------------------------------------------------------------------
// Convenience wrappers — variadic versions
// ---------------------------------------------------------------------------

template <typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void trace(fmt::format_string<Args...> fmt_str, Args&&... args) {
  spdlog::trace(fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void debug(fmt::format_string<Args...> fmt_str, Args&&... args) {
  spdlog::debug(fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void info(fmt::format_string<Args...> fmt_str, Args&&... args) {
  spdlog::info(fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void warn(fmt::format_string<Args...> fmt_str, Args&&... args) {
  spdlog::warn(fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void error(fmt::format_string<Args...> fmt_str, Args&&... args) {
  spdlog::error(fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void critical(fmt::format_string<Args...> fmt_str, Args&&... args) {
  spdlog::critical(fmt_str, std::forward<Args>(args)...);
}

// ---------------------------------------------------------------------------
// Source-location aware logging
// ---------------------------------------------------------------------------

/// Log with automatic source location capture for debugging.
///
///   rpc::log::info_at("connection lost");                // captures file:line
///   rpc::log::log_at<Level::error>("code {}", err_code); // generic version
///
template <Level Lvl, typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void log_at(fmt::format_string<Args...> fmt_str, Args&&... args,
                   const std::source_location loc = std::source_location::current()) {
  spdlog::log(to_spdlog(Lvl), "[{}:{}] {}", loc.file_name(), loc.line(),
              fmt::format(fmt_str, std::forward<Args>(args)...));
}

inline void info_at(std::string_view message,
                    const std::source_location loc = std::source_location::current()) {
  spdlog::info("[{}:{}] {}", loc.file_name(), loc.line(), message);
}

inline void warn_at(std::string_view message,
                    const std::source_location loc = std::source_location::current()) {
  spdlog::warn("[{}:{}] {}", loc.file_name(), loc.line(), message);
}

inline void error_at(std::string_view message,
                     const std::source_location loc = std::source_location::current()) {
  spdlog::error("[{}:{}] {}", loc.file_name(), loc.line(), message);
}

// ---------------------------------------------------------------------------
// Compile-time level gate — zero-cost elimination of disabled log calls
// ---------------------------------------------------------------------------

/// Minimum level set at compile time. Calls below this level become no-ops.
///
///   rpc::log::guarded<Level::debug, Level::info>("skip me");  // no-op
///   rpc::log::guarded<Level::info,  Level::info>("visible");  // emitted
///
template <Level MsgLvl, Level MinLvl, typename... Args>
  requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
inline void guarded(fmt::format_string<Args...> fmt_str, Args&&... args) {
  if constexpr (static_cast<std::uint8_t>(MsgLvl) >= static_cast<std::uint8_t>(MinLvl)) {
    log<MsgLvl>(fmt_str, std::forward<Args>(args)...);
  }
  // else: compiled away entirely
}

// ---------------------------------------------------------------------------
// Scoped context — RAII prefix tag for structured logging
// ---------------------------------------------------------------------------

/// Pushes a contextual prefix while in scope, pops on destruction.
///
///   {
///     auto ctx = rpc::log::ScopedContext("Network");
///     rpc::log::info("connected");   // prints: [Network] connected
///   }
///   rpc::log::info("back to normal");
///
class ScopedContext {
public:
  explicit ScopedContext(std::string_view tag)
      : previous_pattern_(get_current_pattern()) {
    std::string new_pattern =
        fmt::format("[%H:%M:%S] [%^%l%$] [{}] %v", tag);
    spdlog::set_pattern(new_pattern);
  }

  ~ScopedContext() { spdlog::set_pattern(previous_pattern_); }

  // Non-copyable, non-movable
  ScopedContext(const ScopedContext&) = delete;
  ScopedContext& operator=(const ScopedContext&) = delete;
  ScopedContext(ScopedContext&&) = delete;
  ScopedContext& operator=(ScopedContext&&) = delete;

private:
  std::string previous_pattern_;

  [[nodiscard]] static std::string get_current_pattern() {
    // spdlog doesn't expose the current pattern, so we store our default.
    return "[%H:%M:%S] [%^%l%$] %v";
  }
};

// ---------------------------------------------------------------------------
// Tagged logger — a lightweight wrapper with a fixed prefix
// ---------------------------------------------------------------------------

/// A statically-tagged logger for component-based logging.
///
///   constexpr auto net_log = rpc::log::TaggedLogger("Network");
///   net_log.info("port {} open", 8080);
///   net_log.error("timeout on {}", host);
///
template <std::size_t N>
class TaggedLogger {
public:
  constexpr explicit TaggedLogger(const char (&tag)[N]) {
    for (std::size_t i = 0; i < N; ++i) tag_[i] = tag[i];
  }

  template <typename... Args>
    requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
  void trace(fmt::format_string<Args...> fmt_str, Args&&... args) const {
    emit<Level::trace>(fmt_str, std::forward<Args>(args)...);
  }

  template <typename... Args>
    requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
  void debug(fmt::format_string<Args...> fmt_str, Args&&... args) const {
    emit<Level::debug>(fmt_str, std::forward<Args>(args)...);
  }

  template <typename... Args>
    requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
  void info(fmt::format_string<Args...> fmt_str, Args&&... args) const {
    emit<Level::info>(fmt_str, std::forward<Args>(args)...);
  }

  template <typename... Args>
    requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
  void warn(fmt::format_string<Args...> fmt_str, Args&&... args) const {
    emit<Level::warn>(fmt_str, std::forward<Args>(args)...);
  }

  template <typename... Args>
    requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
  void error(fmt::format_string<Args...> fmt_str, Args&&... args) const {
    emit<Level::error>(fmt_str, std::forward<Args>(args)...);
  }

  template <typename... Args>
    requires (sizeof...(Args) == 0 || (Loggable<Args> && ...))
  void critical(fmt::format_string<Args...> fmt_str, Args&&... args) const {
    emit<Level::critical>(fmt_str, std::forward<Args>(args)...);
  }

private:
  char tag_[N]{};

  [[nodiscard]] constexpr std::string_view tag_view() const noexcept {
    return {tag_, N - 1}; // exclude null terminator
  }

  template <Level Lvl, typename... Args>
  void emit(fmt::format_string<Args...> fmt_str, Args&&... args) const {
    spdlog::log(to_spdlog(Lvl), "[{}] {}", tag_view(),
                fmt::format(fmt_str, std::forward<Args>(args)...));
  }
};

// CTAD deduction guide
template <std::size_t N>
TaggedLogger(const char (&)[N]) -> TaggedLogger<N>;

} // namespace rpc::log
