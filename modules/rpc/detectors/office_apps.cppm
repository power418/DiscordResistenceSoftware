#pragma once

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

#include <modules/rpc/activity.cppm>
#include <modules/rpc/os/active_window.cppm>

namespace rpc::detectors {

struct OfficeAppProfile {
  std::string_view display_name;
  std::string_view details;
  std::string_view state;
};

[[nodiscard]] inline std::string office_lower_copy(std::string_view value) {
  std::string lowered(value);
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return lowered;
}

[[nodiscard]] inline bool
office_contains_any(std::string_view haystack,
                    std::initializer_list<std::string_view> needles) {
  for (std::string_view needle : needles) {
    if (!needle.empty() && haystack.find(needle) != std::string_view::npos) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] inline std::string
office_identity(const rpc::ActiveWindowInfo &snapshot) {
  std::string identity;
  identity.reserve(snapshot.process_name.size() + snapshot.exe_path.size() +
                   snapshot.title.size() + 2);
  identity.append(office_lower_copy(snapshot.process_name));
  identity.push_back('\n');
  identity.append(office_lower_copy(snapshot.exe_path));
  identity.push_back('\n');
  identity.append(office_lower_copy(snapshot.title));
  return identity;
}

[[nodiscard]] inline std::optional<OfficeAppProfile>
match_office_app(const rpc::ActiveWindowInfo &snapshot) {
  const std::string identity = office_identity(snapshot);

  // Microsoft Office
  if (office_contains_any(identity, {"winword.exe", "microsoft word"})) {
    return OfficeAppProfile{"Microsoft Word", "Writing a document", "Word"};
  }
  if (office_contains_any(identity, {"excel.exe", "microsoft excel"})) {
    return OfficeAppProfile{"Microsoft Excel", "Editing a spreadsheet",
                            "Excel"};
  }
  if (office_contains_any(identity, {"powerpnt.exe", "microsoft powerpoint"})) {
    return OfficeAppProfile{"Microsoft PowerPoint", "Creating a presentation",
                            "PowerPoint"};
  }
  if (office_contains_any(identity, {"outlook.exe", "microsoft outlook"})) {
    return OfficeAppProfile{"Microsoft Outlook", "Managing emails", "Outlook"};
  }

  // WPS Office
  if (office_contains_any(identity, {"wps.exe", "wps office"})) {
    return OfficeAppProfile{"WPS Office Writer", "Writing a document",
                            "WPS Writer"};
  }
  if (office_contains_any(identity, {"et.exe"})) {
    return OfficeAppProfile{"WPS Office Spreadsheets", "Editing a spreadsheet",
                            "WPS Spreadsheets"};
  }
  if (office_contains_any(identity, {"wpp.exe"})) {
    return OfficeAppProfile{"WPS Office Presentation",
                            "Creating a presentation", "WPS Presentation"};
  }

  // OnlyOffice
  if (office_contains_any(identity, {"editors.exe", "onlyoffice"})) {
    return OfficeAppProfile{"ONLYOFFICE", "Editing documents", "ONLYOFFICE"};
  }

  // LibreOffice & OpenOffice
  if (office_contains_any(identity, {"swriter.exe", "libreoffice writer",
                                     "openoffice writer"})) {
    return OfficeAppProfile{"Office Writer", "Writing a document", "Writer"};
  }
  if (office_contains_any(
          identity, {"scalc.exe", "libreoffice calc", "openoffice calc"})) {
    return OfficeAppProfile{"Office Calc", "Editing a spreadsheet", "Calc"};
  }
  if (office_contains_any(identity, {"simpress.exe", "libreoffice impress",
                                     "openoffice impress"})) {
    return OfficeAppProfile{"Office Impress", "Creating a presentation",
                            "Impress"};
  }
  if (office_contains_any(identity, {"soffice.bin", "soffice.exe",
                                     "libreoffice", "openoffice"})) {
    return OfficeAppProfile{"Office Suite", "Working on documents", "Office"};
  }

  // Text Editors
  if (office_contains_any(identity, {"notepad.exe"})) {
    return OfficeAppProfile{"Notepad", "Editing text", "Notepad"};
  }
  if (office_contains_any(identity, {"notepad++.exe", "notepad++"})) {
    return OfficeAppProfile{"Notepad++", "Editing source code or text",
                            "Notepad++"};
  }

  // PDF Readers
  if (office_contains_any(identity, {"acrord32.exe", "acrobat.exe",
                                     "adobe acrobat", "adobe reader"})) {
    return OfficeAppProfile{"Adobe Acrobat Reader", "Reading a PDF",
                            "Adobe Reader"};
  }
  if (office_contains_any(identity, {"okular.exe", "okular"})) {
    return OfficeAppProfile{"Okular", "Reading a PDF", "Okular"};
  }
  if (office_contains_any(identity, {"foxitreader.exe", "foxit reader"})) {
    return OfficeAppProfile{"Foxit Reader", "Reading a PDF", "Foxit Reader"};
  }
  if (office_contains_any(identity, {"sumatrapdf.exe", "sumatrapdf"})) {
    return OfficeAppProfile{"SumatraPDF", "Reading a PDF", "SumatraPDF"};
  }
  if (office_contains_any(identity, {"pdfxedit.exe", "pdf-xchange"})) {
    return OfficeAppProfile{"PDF-XChange Editor", "Reading a PDF",
                            "PDF-XChange"};
  }
  if (office_contains_any(identity, {"nitropdf.exe", "nitro pdf"})) {
    return OfficeAppProfile{"Nitro PDF", "Reading a PDF", "Nitro PDF"};
  }

  return std::nullopt;
}

[[nodiscard]] inline std::string
extract_office_project(std::string_view title,
                       [[maybe_unused]] std::string_view app_name) {
  if (title.empty())
    return {};
  const auto sep = title.rfind(" - ");
  if (sep == std::string_view::npos || sep == 0)
    return {};

  std::string project(title.substr(0, sep));
  while (!project.empty() &&
         (project.back() == ' ' || project.back() == '\t')) {
    project.pop_back();
  }
  return project;
}

[[nodiscard]] inline std::optional<rpc::ActivityPayload>
detect_office_activity(const rpc::ActiveWindowInfo &snapshot) {
  const auto profile = match_office_app(snapshot);
  if (!profile.has_value()) {
    return std::nullopt;
  }

  rpc::ActivityPayload activity{};
  activity.state = std::string(profile->state);
  activity.start_timestamp_unix = snapshot.start_timestamp_unix;

  std::string project =
      extract_office_project(snapshot.title, profile->display_name);
  if (!project.empty()) {
    activity.details = project;
  } else {
    activity.details = std::string(profile->details);
  }

  return activity;
}

} // namespace rpc::detectors
