module;

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

export module rpc.detectors.creative_apps;

import rpc.activity;
import rpc.os.active_window;

export namespace rpc::detectors {

struct CreativeAppProfile {
  std::string_view display_name;
  std::string_view details;
  std::string_view state;
};

[[nodiscard]] inline std::string lower_copy(std::string_view value) {
  std::string lowered(value);
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return lowered;
}

[[nodiscard]] inline bool contains_any(std::string_view haystack,
                                       std::initializer_list<std::string_view> needles) {
  for (std::string_view needle : needles) {
    if (!needle.empty() && haystack.find(needle) != std::string_view::npos) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] inline std::string creative_identity(const rpc::ActiveWindowInfo& snapshot) {
  std::string identity;
  identity.reserve(snapshot.process_name.size() + snapshot.exe_path.size() + snapshot.title.size() + 2);
  identity.append(lower_copy(snapshot.process_name));
  identity.push_back('\n');
  identity.append(lower_copy(snapshot.exe_path));
  identity.push_back('\n');
  identity.append(lower_copy(snapshot.title));
  return identity;
}

[[nodiscard]] inline std::optional<CreativeAppProfile>
match_3d_app(std::string_view identity) {
  if (contains_any(identity, {"blender"})) {
    return CreativeAppProfile{
      .display_name = "Blender",
      .details = "Modeling in 3D",
      .state = "Blender",
    };
  }

  if (contains_any(identity, {"maya.exe", "autodesk maya", "maya 202", "maya 201"})) {
    return CreativeAppProfile{
      .display_name = "Autodesk Maya",
      .details = "Creating 3D scenes",
      .state = "Maya",
    };
  }

  if (contains_any(identity, {"3dsmax", "3ds max", "autodesk 3ds max"})) {
    return CreativeAppProfile{
      .display_name = "Autodesk 3ds Max",
      .details = "Modeling in 3D",
      .state = "3ds Max",
    };
  }

  if (contains_any(identity, {"cinema 4d", "c4d.exe", "maxon cinema 4d"})) {
    return CreativeAppProfile{
      .display_name = "Cinema 4D",
      .details = "Building motion graphics",
      .state = "Cinema 4D",
    };
  }

  if (contains_any(identity, {"houdini", "houdinifx", "houdini engine"})) {
    return CreativeAppProfile{
      .display_name = "Houdini",
      .details = "Building procedural worlds",
      .state = "Houdini",
    };
  }

  if (contains_any(identity, {"zbrush"})) {
    return CreativeAppProfile{
      .display_name = "ZBrush",
      .details = "Sculpting characters",
      .state = "ZBrush",
    };
  }

  if (contains_any(identity, {"sketchup", "trimble sketchup"})) {
    return CreativeAppProfile{
      .display_name = "SketchUp",
      .details = "Designing 3D spaces",
      .state = "SketchUp",
    };
  }

  if (contains_any(identity, {"ue4editor", "ue5editor", "unreal editor", "unrealengine"})) {
    return CreativeAppProfile{
      .display_name = "Unreal Engine",
      .details = "Building 3D worlds",
      .state = "Unreal Engine",
    };
  }

  if (contains_any(identity, {"unity editor", "unity.exe", "unity hub"})) {
    return CreativeAppProfile{
      .display_name = "Unity",
      .details = "Building real-time 3D",
      .state = "Unity",
    };
  }

  if (contains_any(identity, {"godot"})) {
    return CreativeAppProfile{
      .display_name = "Godot",
      .details = "Building game worlds",
      .state = "Godot",
    };
  }

  if (contains_any(identity, {"marmoset", "toolbag"})) {
    return CreativeAppProfile{
      .display_name = "Marmoset Toolbag",
      .details = "Rendering assets",
      .state = "Toolbag",
    };
  }

  if (contains_any(identity, {"daz studio", "dazstudio"})) {
    return CreativeAppProfile{
      .display_name = "DAZ Studio",
      .details = "Posing and rendering",
      .state = "DAZ Studio",
    };
  }

  return std::nullopt;
}

[[nodiscard]] inline std::optional<CreativeAppProfile>
match_creative_app(const rpc::ActiveWindowInfo& snapshot) {
  const std::string identity = creative_identity(snapshot);

  if (contains_any(identity, {"ableton", "live 12", "live 11", "live 10"})) {
    return CreativeAppProfile{
      .display_name = "Ableton Live",
      .details = "Producing music",
      .state = "Ableton Live",
    };
  }

  if (contains_any(identity, {"fl64.exe", "fl studio", "image-line"})) {
    return CreativeAppProfile{
      .display_name = "FL Studio",
      .details = "Producing music",
      .state = "FL Studio",
    };
  }

  if (auto profile = match_3d_app(identity); profile.has_value()) {
    return profile;
  }

  if (contains_any(identity, {"krita"})) {
    return CreativeAppProfile{
      .display_name = "Krita",
      .details = "Creating artwork",
      .state = "Krita",
    };
  }

  if (contains_any(identity, {"photoshop"})) {
    return CreativeAppProfile{
      .display_name = "Adobe Photoshop",
      .details = "Designing visuals",
      .state = "Photoshop",
    };
  }

  if (contains_any(identity, {"illustrator"})) {
    return CreativeAppProfile{
      .display_name = "Adobe Illustrator",
      .details = "Creating vector art",
      .state = "Illustrator",
    };
  }

  if (contains_any(identity, {"premiere pro", "adobe premiere"})) {
    return CreativeAppProfile{
      .display_name = "Adobe Premiere Pro",
      .details = "Editing video",
      .state = "Premiere Pro",
    };
  }

  if (contains_any(identity, {"afterfx", "after effects", "adobe after effects"})) {
    return CreativeAppProfile{
      .display_name = "Adobe After Effects",
      .details = "Creating motion graphics",
      .state = "After Effects",
    };
  }

  if (contains_any(identity, {"lightroom"})) {
    return CreativeAppProfile{
      .display_name = "Adobe Lightroom",
      .details = "Editing photos",
      .state = "Lightroom",
    };
  }

  if (contains_any(identity, {"audition"})) {
    return CreativeAppProfile{
      .display_name = "Adobe Audition",
      .details = "Editing audio",
      .state = "Audition",
    };
  }

  if (contains_any(identity, {"indesign"})) {
    return CreativeAppProfile{
      .display_name = "Adobe InDesign",
      .details = "Designing layouts",
      .state = "InDesign",
    };
  }

  if (contains_any(identity, {"media encoder"})) {
    return CreativeAppProfile{
      .display_name = "Adobe Media Encoder",
      .details = "Encoding media",
      .state = "Media Encoder",
    };
  }

  if (contains_any(identity, {"bridge.exe", "adobe bridge"})) {
    return CreativeAppProfile{
      .display_name = "Adobe Bridge",
      .details = "Managing creative assets",
      .state = "Bridge",
    };
  }

  if (contains_any(identity, {"substance 3d", "substance painter", "substance designer"})) {
    return CreativeAppProfile{
      .display_name = "Adobe Substance 3D",
      .details = "Texturing 3D assets",
      .state = "Substance 3D",
    };
  }

  if (contains_any(identity, {"adobe"})) {
    return CreativeAppProfile{
      .display_name = "Adobe Creative App",
      .details = "Working creatively",
      .state = "Adobe",
    };
  }

  return std::nullopt;
}

// ---------------------------------------------------------------------------
// Title parsing — extract project/file name from window title
// ---------------------------------------------------------------------------
//
// Most creative apps use the pattern:   "<project> - <app name>"
//   "Untitled - Ableton Live 12 Suite"
//   "My Song.flp - FL Studio 2024"
//   "logo.psd @ 100% (RGB/8) - Adobe Photoshop"
//   "drawing.kra - Krita"
//
// We extract the part before the last " - " as the project/document name.

[[nodiscard]] inline std::string extract_project_name(std::string_view title,
                                                       std::string_view app_display_name) {
  if (title.empty()) return {};

  // Find the last " - " separator
  const auto sep = title.rfind(" - ");
  if (sep == std::string_view::npos || sep == 0) {
    return {};  // No separator or nothing before it
  }

  std::string project(title.substr(0, sep));

  // Trim trailing whitespace
  while (!project.empty() && (project.back() == ' ' || project.back() == '\t')) {
    project.pop_back();
  }

  // Don't return the project name if it's just the app name again
  if (lower_copy(project) == lower_copy(app_display_name)) {
    return {};
  }

  return project;
}

[[nodiscard]] inline std::optional<rpc::ActivityPayload>
detect_creative_activity(const rpc::ActiveWindowInfo& snapshot) {
  const auto profile = match_creative_app(snapshot);
  if (!profile.has_value()) {
    return std::nullopt;
  }

  rpc::ActivityPayload activity{};
  activity.state = std::string(profile->state);
  activity.start_timestamp_unix = snapshot.start_timestamp_unix;

  // Try to extract the project/file name from the window title
  std::string project = extract_project_name(snapshot.title, profile->display_name);
  if (!project.empty()) {
    activity.details = project;
  } else {
    // Fallback to the generic description
    activity.details = std::string(profile->details);
  }

  return activity;
}

} // namespace rpc::detectors
