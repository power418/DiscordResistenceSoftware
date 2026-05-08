# PRD — Discord Rich Presence Multi-App

## 1. Product Summary

This product is a desktop application/service that displays Discord Rich Presence based on the applications the user is currently using, such as Okular, Ableton Live, FL Studio, Adobe apps, browsers, editors, media players, and more. The system must be scalable, easy to add support for new applications, and stable running in the background without interrupting the user's workflow.

The main goal is not to create one-by-one hardcoded integrations, but to build an RPC platform based on plugins/detectors so that new apps can be added via configuration or small modules.

---

## 2. Problems to Solve

Users want their Discord status to automatically show activity from many different applications. Issues that arise if built simply:

- Each application has a different way of detection.
- Window/process names are not always consistent.
- Some applications have readable metadata, some do not.
- Discord RPC needs to be updated with rate-limits to avoid spam.
- The application must remain lightweight even while monitoring many apps.
- Support for new applications should be addable without rewriting the core.

---

## 3. Target User

### Primary User

- Music creators using Ableton Live, FL Studio, Reaper, Logic, or other DAWs.
- Designers/editors using Adobe Photoshop, Illustrator, Premiere Pro, After Effects, Lightroom.
- Developers, readers, and power users using Okular, VS Code, browsers, terminals, and other productivity apps.

### Secondary User

- Discord communities wanting a more personal presence.
- Plugin developers wanting to add detectors for specific applications.

---

## 4. Goals

1. Display Discord Rich Presence corresponding to the active application.
2. Support many applications with a plugin architecture.
3. Provide fallback detection based on process/window title.
4. Provide advanced detectors for applications with richer metadata.
5. Cross-platform support for at least Windows and Linux, with macOS as a future option.
6. Safe RPC updates (no spam) and no crashes if Discord is not open.
7. User-configurable without needing to code.

---

## 5. Non-Goals

- Do not aggressively read private files without user permission.
- Do not use Discord user tokens or selfbots.
- Do not send user data to external servers in the MVP.
- Do not guarantee detailed metadata for all applications from the start.
- Do not create official plugins for all applications at once.

---

## 6. Product Principles

1. **Local-first** — all detection runs locally.
2. **Plugin-first** — app support is made modular.
3. **Privacy by default** — users can choose if project/file names are shown.
4. **Graceful degradation** — if detailed metadata fails, show a simple status.
5. **Low resource usage** — polling must be CPU/memory efficient.
6. **Extensible** — support for new apps via config or plugins.

---

## 7. Use Cases

### UC-01 — User opens Ableton Live

The system detects Ableton as the active process and sends presence:

- State: Producing music
- Details: Ableton Live
- Large Image: ableton
- Timestamp: since Ableton became active

If project metadata is available and permitted by the user:

- Details: Working on `Project Name`
- State: Ableton Live

### UC-02 — User opens Okular

The system detects Okular and reads the window title if available:

- Details: Reading PDF
- State: Okular
- Optional: document name if permitted by the user

### UC-03 — User switches to Photoshop

The system switches the Discord presence from Ableton to Photoshop after a few seconds of debounce to avoid flickering.

### UC-04 — Unknown App

If the user enables generic mode:

- Details: Working in `{App Name}`
- State: Active

If generic mode is off, the presence is cleared or kept as the last app according to settings.

---

## 8. Functional Requirements

### FR-01 — Discord RPC Connector

The system must be able to connect to the Discord desktop client via local IPC/RPC.

Requirements:
- Connect when the app starts.
- Auto-reconnect if Discord is closed/reopened.
- Clear activity on app exit.
- Rate-limit presence updates.
- Queue updates to avoid race conditions.

Acceptance Criteria:
- When Discord is active, presence appears within seconds after the target app becomes active.
- When Discord is off, the application does not crash.
- When Discord is reopened, presence reconnects automatically.

---

### FR-02 — Active App Detector

The system must know which application/window is currently active.

Minimum metadata:
- Process name
- Window title
- Executable path if available
- OS platform
- Active start timestamp

Platform strategy:
- Windows: Win32 foreground window API.
- Linux X11: xprop/wmctrl or X11 bindings.
- Linux Wayland: limited fallback; use portals or process/window heuristics if available.
- macOS: Accessibility API in a future phase.

Acceptance Criteria:
- Can detect at least the active process on Windows and Linux.
- Can debounce window changes so status doesn't change too rapidly.

---

### FR-03 — Plugin/Detector System

Every app support is built as a plugin with a standard contract.

Conceptual interface:
```ts
interface AppDetector {
  id: string;
  displayName: string;
  match(input: ActiveWindowSnapshot): boolean;
  extract(input: ActiveWindowSnapshot): Promise<ActivityPayload>;
  priority: number;
  capabilities: DetectorCapability[];
}
```

Example plugins:
- `okular.detector`
- `ableton.detector`
- `flstudio.detector`
- `adobe-photoshop.detector`
- `adobe-premiere.detector`
- `generic-process.detector`

Acceptance Criteria:
- Adding a new plugin doesn't require changing the RPC connector.
- Plugins can be enabled/disabled via config.
- If a plugin errors, the core remains running and falls back to a generic detector.

---

### FR-04 — Config System

Users can adjust behavior without coding.

Minimum config:
```json
{
  "privacy": {
    "showFileName": false,
    "showProjectName": true,
    "showElapsedTime": true
  },
  "discord": {
    "clientId": "YOUR_CLIENT_ID",
    "updateIntervalMs": 15000,
    "debounceMs": 3000
  },
  "apps": {
    "ableton": { "enabled": true },
    "flstudio": { "enabled": true },
    "okular": { "enabled": true },
    "adobe": { "enabled": true },
    "generic": { "enabled": false }
  }
}
```

Acceptance Criteria:
- Config is read at startup.
- Invalid config doesn't crash the app; use safe defaults.
- Privacy settings are respected by all detectors.

---

### FR-05 — Activity Mapping

The system must transform detection results into a Discord Rich Presence payload.

Common fields:
- `details`
- `state`
- `largeImageKey`
- `largeImageText`
- `smallImageKey`
- `smallImageText`
- `startTimestamp`
- `buttons` (optional)

Rules:
- Do not update if the payload is identical to the previous one.
- Do not show filename/project if privacy settings forbid it.
- Use image assets from the Discord Developer Portal matching the `clientId`.

Acceptance Criteria:
- Valid payload is sent to Discord.
- Duplicate payloads are not sent repeatedly.
- Presence changes when the active app changes.

---

### FR-06 — App Registry

The system has a registry for app lists and their matching methods.

Example entry:
```json
{
  "id": "flstudio",
  "displayName": "FL Studio",
  "processNames": ["FL64.exe", "FL Studio.exe"],
  "windowTitlePatterns": ["FL Studio"],
  "largeImageKey": "flstudio",
  "category": "music"
}
```

Acceptance Criteria:
- Registry can be updated without recompiling for the generic detector.
- Specific plugins can override the generic registry.

---

## 9. Non-Functional Requirements

### Performance
- Target idle CPU: low, ideally below 1–3% on normal devices.
- Active window polling: 1–3 seconds or event-driven if supported by the OS.
- Discord update interval: minimum 10–15 seconds unless there's a major change.

### Reliability
- Auto-reconnect Discord IPC.
- Plugin isolation: plugin errors don't crash the core.
- Structured logging for debugging.

### Security & Privacy
- Do not use Discord user tokens.
- Do not read document/audio/project contents unless a plugin specifically needs it and the user permits.
- All data remains local in the MVP.
- File/project names are hidden by default for sensitive applications.

### Maintainability
- The core does not know specific application details.
- App-specific logic resides in plugins.
- Unit tests for matchers and mappers.
- Integration tests for the Discord RPC connector with mock IPC.

---

## 10. System Architecture

### High-Level Architecture
```text
+-------------------------+
| Desktop Tray / CLI App  |
+-----------+-------------+
            |
            v
+-------------------------+
| Core Orchestrator       |
| - lifecycle             |
| - scheduler             |
| - debounce              |
| - state machine         |
+-----------+-------------+
            |
   +--------+---------+
   |                  |
   v                  v
+----------+     +----------------+
| OS Layer |     | Config Manager |
| Active   |     | Privacy Rules  |
| Window   |     +----------------+
+----+-----+
     |
     v
+-------------------------+
| Detector Manager        |
| - plugin registry       |
| - priority matching     |
| - fallback detector     |
+-----------+-------------+
            |
            v
+-------------------------+
| Activity Mapper         |
| normalize payload       |
+-----------+-------------+
            |
            v
+-------------------------+
| Discord RPC Adapter     |
| IPC connect/reconnect   |
+-------------------------+
```

---

## 11. Technical Components

### 11.1 Core Orchestrator
Tasks:
- Run app lifecycle.
- Schedule polling or event subscriptions.
- Store current state.
- Compare new activity with old activity.
- Handle debounce and rate-limiting.

State machine:
```text
STARTING -> DISCORD_CONNECTING -> READY -> DETECTING -> UPDATING_RPC
                             \-> DISCORD_UNAVAILABLE -> RETRYING
```

---

### 11.2 OS Integration Layer
Tasks:
- Get active window.
- Get process info.
- Get window title.
- Normalize data across OSs.

Standard output:
```ts
interface ActiveWindowSnapshot {
  platform: "windows" | "linux" | "macos";
  processName: string;
  executablePath?: string;
  windowTitle?: string;
  pid?: number;
  capturedAt: number;
}
```

---

### 11.3 Detector Manager
Tasks:
- Load plugins from internal folders.
- Sort plugins by priority.
- Run `match()` for the active snapshot.
- Run `extract()` on the matching detector.
- Fallback to the generic detector.

Example priorities:
1. App-specific plugin with detailed metadata.
2. Registry-based detector.
3. Generic process detector.

---

### 11.4 Discord RPC Adapter
Tasks:
- Connect to Discord local IPC.
- Send `SET_ACTIVITY`.
- Clear activity on shutdown.
- Auto-reconnect.
- Handle IPC errors.

Internal payload:
```ts
interface ActivityPayload {
  appId: string;
  details: string;
  state?: string;
  largeImageKey?: string;
  largeImageText?: string;
  smallImageKey?: string;
  smallImageText?: string;
  startTimestamp?: number;
  buttons?: Array<{ label: string; url: string }>;
}
```

---

## 12. Tech Stack Choices

### Main Option — C++ Native Desktop (Recommended per requirement)
Since the target implementation uses C++ and runs as a desktop app/background service, the architecture will focus on the native layer.

Components:
- C++17/20 as the core language.
- OS-specific APIs for active window detection.
- Discord RPC via native IPC (pipe/socket) or a wrapper library.
- Optional lightweight GUI (tray) using Qt / Win32 / GTK.

Pros:
- Very high performance and low memory.
- Full control over OS-level APIs.
- Suitable for long-term background daemons/services.
- No additional runtime needed (like Node.js).

Cons:
- More complex development.
- Plugin system is harder than JS/TS.
- More effort for cross-platform handling.

---

### Architecture Structure (C++)
```text
src/
  core/
    orchestrator/
    state_machine/
    scheduler/

  os/
    windows/
      active_window.cpp
    linux/
      x11_window.cpp

  rpc/
    discord_client.cpp
    ipc_transport.cpp

  detectors/
    base_detector.hpp
    registry_detector.cpp
    ableton_detector.cpp
    flstudio_detector.cpp
    adobe_detector.cpp
    okular_detector.cpp

  config/
    config_loader.cpp
    config_schema.hpp

  utils/
    logger.cpp
    time.cpp

  app/
    main.cpp
    tray.cpp (optional)
```

---

## 13. Required Libraries

### Core C++ Libraries
- `nlohmann/json` → JSON config parsing
- `spdlog` → fast and lightweight logging
- `fmt` → modern string formatting
- `asio` or `boost::asio` → async event loop (optional)
- `filesystem` (std) → file handling

---

### Discord RPC
Options:
1. **discord-rpc (official legacy)**
   - C-based library
   - Easy to use in C++
2. **Custom IPC implementation (recommended advanced)**
   - Direct communication with Discord IPC pipe (`\?\pipe\discord-ipc-0` on Windows)
   - More flexible and future-proof
3. **Third-party C++ wrapper**
   - Wrapper around discord-rpc or IPC

---

### OS Integration
#### Windows
- Win32 API:
  - `GetForegroundWindow`
  - `GetWindowText`
  - `GetWindowThreadProcessId`
  - `OpenProcess`
  - `QueryFullProcessImageName`

#### Linux (X11)
- `Xlib` / `X11`
- Fallback tools:
  - `xprop`
  - `wmctrl`

#### Linux (Wayland)
- Highly limited → fallback to process-based detection

---

### Desktop UI (Optional)
- **Qt (recommended if UI is needed)**
  - Tray icon
  - Settings window
- Alternatives:
  - Win32 tray API (Windows only)
  - GTK (Linux)

---

### Build System
- `CMake` (required)
- `vcpkg` or `conan` for dependency management

---

### Testing
- `GoogleTest`
- `Catch2`

---

### Plugin System (C++)
Two approaches:
#### A. Static Plugins (Recommended MVP)
- All detectors compiled into the binary
- JSON registry for mapping
- Simple and stable

#### B. Dynamic Plugins (Advanced)
- Load `.dll` / `.so`
- Interface via abstract class
```cpp
class AppDetector {
public:
    virtual bool match(const ActiveWindowSnapshot&) = 0;
    virtual ActivityPayload extract(const ActiveWindowSnapshot&) = 0;
};
```
- More flexible but complex

---

### Additional Utilities
- `inih` or lightweight JSON config loader
- `chrono` for timing
- `thread` for scheduler/polling

---

## 14. App Support Strategy

### Level 1 — Generic Support
Support based on process/window title.
Suitable for:
- Okular, Adobe apps, FL Studio, Ableton, VS Code, Browser, Terminal.
Simple but stable output.

### Level 2 — Smart Window Title Parsing
Support based on title patterns.
Examples:
- Okular: get PDF name from window title.
- Photoshop: get active filename from title.
- Ableton/FL Studio: get project name if it appears in the title.

### Level 3 — Deep Integration
Support through specific APIs/plugins if the application provides an API, scripting, or local state.
Examples:
- DAW plugin/bridge for Ableton/FL Studio for BPM, playing/paused, project name.
- Adobe scripting/UXP/CEP for more detailed document metadata.

MVP is sufficient with Level 1 + some Level 2.

---

## 15. MVP Scope

### In Scope
- Windows + Linux X11 support.
- Discord RPC connect/reconnect.
- Active window detection.
- Simple plugin manager.
- Built-in detectors for: Okular, Ableton Live, FL Studio, Adobe Photoshop, Adobe Illustrator, Adobe Premiere Pro, Adobe After Effects, Generic app detector.
- JSON Config.
- Privacy settings.
- Minimal tray icon.
- Local log file.

### Out of Scope MVP
- Deep DAW metadata like BPM/track/playing state.
- Adobe UXP/CEP plugins.
- Cloud sync.
- Plugin marketplace.
- Mobile support.

---

## 16. Milestones

### M1 — Proof of Concept
Deliverables:
- Discord RPC successfully displayed.
- Active app detection working on one OS.
- Hardcoded detectors for 2 apps.

### M2 — MVP Architecture
Deliverables:
- Plugin interface.
- Detector manager.
- Config manager.
- Activity mapper.
- Initial built-in detectors.

### M3 — Desktop App
Deliverables:
- Tray app.
- Autostart option.
- Settings file.
- Simple logs viewer.

### M4 — App Expansion
Deliverables:
- Support for Okular, Ableton, FL Studio, Adobe suite.
- Registry-based app definitions.
- Privacy controls.

### M5 — Stabilization
Deliverables:
- Reconnect handling.
- Rate-limit testing.
- Packaging installer.
- Plugin authoring documentation.

---

## 17. Success Metrics
- RPC appears correctly for at least 8 target applications.
- App runs 8 hours without crashing.
- Low idle CPU.
- Discord reconnect succeeds after Discord restart.
- Adding a new app via registry takes less than 10 minutes.
- 90% of active app changes detected within 3–5 seconds.

---

## 18. Risks & Mitigations

### Risk: Wayland limits access to active window
Mitigation:
- Focus MVP on Windows and Linux X11.
- Provide manual fallback/app whitelist.
- Document Wayland limitations.

### Risk: Discord RPC unstable during Discord restart
Mitigation:
- Implement exponential backoff reconnect loop.
- Do not crash when IPC is unavailable.

### Risk: Privacy concerns due to filename display
Mitigation:
- Default `showFileName = false`.
- Per-app privacy overrides.
- Preview payload before sending.

### Risk: Too many hardcoded plugins
Mitigation:
- JSON Registry for generic apps.
- Plugins only for apps needing special parsing.
