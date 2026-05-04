module;

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

export module rpc.detectors.productive_apps;

import rpc.activity;
import rpc.os.active_window;

export namespace rpc::detectors {

struct ProductiveAppProfile {
  std::string_view display_name;
  std::string_view details;
  std::string_view state;
};

[[nodiscard]] inline std::string productive_lower_copy(std::string_view value) {
  std::string lowered(value);
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return lowered;
}

[[nodiscard]] inline bool productive_contains_any(std::string_view haystack,
                                                  std::initializer_list<std::string_view> needles) {
  for (std::string_view needle : needles) {
    if (!needle.empty() && haystack.find(needle) != std::string_view::npos) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] inline std::string productive_identity(const rpc::ActiveWindowInfo& snapshot) {
  std::string identity;
  identity.reserve(snapshot.process_name.size() + snapshot.exe_path.size() + snapshot.title.size() + 2);
  identity.append(productive_lower_copy(snapshot.process_name));
  identity.push_back('\n');
  identity.append(productive_lower_copy(snapshot.exe_path));
  identity.push_back('\n');
  identity.append(productive_lower_copy(snapshot.title));
  return identity;
}

[[nodiscard]] inline std::optional<ProductiveAppProfile>
match_productive_app(const rpc::ActiveWindowInfo& snapshot) {
  const std::string identity = productive_identity(snapshot);

  if (productive_contains_any(identity, {"brave.exe", "brave browser",
                                          "bravesoftware\\brave-browser",
                                          "brave-browser"})) {
    return ProductiveAppProfile{
      .display_name = "Brave",
      .details = "Browsing productively",
      .state = "Brave Browser",
    };
  }

  return std::nullopt;
}

[[nodiscard]] inline std::optional<rpc::ActivityPayload>
detect_productive_activity(const rpc::ActiveWindowInfo& snapshot) {
  const auto profile = match_productive_app(snapshot);
  if (!profile.has_value()) {
    return std::nullopt;
  }

  rpc::ActivityPayload activity{};
  activity.details = std::string(profile->details);
  activity.state = std::string(profile->state);
  activity.start_timestamp_unix = snapshot.start_timestamp_unix;
  return activity;
}

} // namespace rpc::detectors
