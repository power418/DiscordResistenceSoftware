# PRD — Discord Rich Presence Multi-App

## 1. Ringkasan Produk

Produk ini adalah aplikasi desktop/service yang menampilkan Discord Rich Presence berdasarkan aplikasi yang sedang digunakan user, seperti Okular, Ableton Live, FL Studio, Adobe apps, browser, editor, media player, dan aplikasi lain. Sistem harus scalable, mudah ditambah support aplikasi baru, dan stabil berjalan di background tanpa mengganggu workflow user.

Tujuan utamanya bukan membuat integrasi satu-per-satu secara hardcoded, tetapi membangun platform RPC berbasis plugin/detector sehingga app baru bisa ditambahkan lewat konfigurasi atau module kecil.

---

## 2. Masalah yang Ingin Diselesaikan

User ingin Discord status otomatis menunjukkan aktivitas dari banyak aplikasi berbeda. Masalah yang muncul kalau dibuat secara sederhana:

- Setiap aplikasi punya cara deteksi berbeda.
- Nama window/process tidak selalu konsisten.
- Beberapa aplikasi punya metadata yang bisa dibaca, sebagian tidak.
- Discord RPC perlu update dengan rate-limit agar tidak spam.
- Aplikasi harus tetap ringan walaupun memonitor banyak app.
- Support aplikasi baru harus bisa ditambahkan tanpa rewrite core.

---

## 3. Target User

### Primary User

- Kreator musik yang memakai Ableton Live, FL Studio, Reaper, Logic, atau DAW lain.
- Designer/editor yang memakai Adobe Photoshop, Illustrator, Premiere Pro, After Effects, Lightroom.
- Developer, reader, dan power user yang memakai Okular, VS Code, browser, terminal, dan app produktivitas lain.

### Secondary User

- Komunitas Discord yang ingin presence lebih personal.
- Developer plugin yang ingin menambah detector untuk aplikasi tertentu.

---

## 4. Goals

1. Menampilkan Discord Rich Presence sesuai aplikasi aktif.
2. Mendukung banyak aplikasi dengan arsitektur plugin.
3. Menyediakan fallback detection berdasarkan process/window title.
4. Menyediakan detector lanjutan untuk aplikasi yang punya metadata lebih kaya.
5. Bisa berjalan cross-platform minimal Windows dan Linux, dengan opsi macOS di fase berikutnya.
6. Update RPC aman, tidak spam, dan tidak crash saat Discord belum terbuka.
7. Bisa dikonfigurasi user tanpa harus coding.

---

## 5. Non-Goals

- Tidak membaca file private secara agresif tanpa izin user.
- Tidak memakai Discord user token atau selfbot.
- Tidak mengirim data user ke server eksternal pada MVP.
- Tidak menjamin metadata detail untuk semua aplikasi sejak awal.
- Tidak membuat plugin resmi untuk semua aplikasi sekaligus.

---

## 6. Prinsip Produk

1. **Local-first** — semua deteksi berjalan lokal.
2. **Plugin-first** — app support dibuat modular.
3. **Privacy by default** — user bisa memilih apakah nama project/file boleh tampil.
4. **Graceful degradation** — kalau metadata detail gagal, tetap tampilkan status sederhana.
5. **Low resource usage** — polling harus hemat CPU/memori.
6. **Extensible** — support app baru bisa lewat config atau plugin.

---

## 7. Use Cases

### UC-01 — User membuka Ableton Live

Sistem mendeteksi Ableton sebagai active process, lalu mengirim presence:

- State: Producing music
- Details: Ableton Live
- Large Image: ableton
- Timestamp: sejak Ableton aktif

Jika metadata project tersedia dan user mengizinkan:

- Details: Working on `Project Name`
- State: Ableton Live

### UC-02 — User membuka Okular

Sistem mendeteksi Okular dan membaca window title bila tersedia:

- Details: Reading PDF
- State: Okular
- Optional: nama dokumen jika diizinkan user

### UC-03 — User berpindah ke Photoshop

Sistem mengganti Discord presence dari Ableton ke Photoshop setelah debounce beberapa detik agar tidak flicker.

### UC-04 — App tidak dikenal

Jika user mengaktifkan generic mode:

- Details: Working in `{App Name}`
- State: Active

Jika generic mode mati, presence dibersihkan atau tetap memakai app terakhir sesuai setting.

---

## 8. Functional Requirements

### FR-01 — Discord RPC Connector

Sistem harus bisa connect ke Discord desktop client melalui local IPC/RPC.

Kebutuhan:

- Connect ketika app start.
- Reconnect otomatis jika Discord ditutup/dibuka ulang.
- Clear activity saat app exit.
- Rate-limit update presence.
- Queue update agar tidak ada race condition.

Acceptance Criteria:

- Saat Discord aktif, presence muncul maksimal beberapa detik setelah app target aktif.
- Saat Discord mati, aplikasi tidak crash.
- Saat Discord dibuka ulang, presence reconnect otomatis.

---

### FR-02 — Active App Detector

Sistem harus mengetahui aplikasi/window yang sedang aktif.

Minimum metadata:

- Process name
- Window title
- Executable path jika tersedia
- OS platform
- Timestamp active start

Platform strategy:

- Windows: Win32 foreground window API.
- Linux X11: xprop/wmctrl atau binding X11.
- Linux Wayland: fallback terbatas; gunakan portal atau process/window heuristics jika tersedia.
- macOS: Accessibility API di fase berikutnya.

Acceptance Criteria:

- Bisa mendeteksi minimal process aktif di Windows dan Linux.
- Bisa debounce perubahan window agar status tidak berubah terlalu cepat.

---

### FR-03 — Plugin/Detector System

Setiap app support dibuat sebagai plugin dengan kontrak standar.

Interface konseptual:

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

Contoh plugin:

- `okular.detector`
- `ableton.detector`
- `flstudio.detector`
- `adobe-photoshop.detector`
- `adobe-premiere.detector`
- `generic-process.detector`

Acceptance Criteria:

- Menambah plugin baru tidak perlu mengubah RPC connector.
- Plugin bisa di-enable/disable lewat config.
- Jika plugin error, core tetap berjalan dan fallback ke generic detector.

---

### FR-04 — Config System

User bisa mengatur behavior tanpa coding.

Config minimum:

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

- Config bisa dibaca saat startup.
- Config invalid tidak membuat app crash; gunakan default aman.
- Setting privacy dihormati oleh semua detector.

---

### FR-05 — Activity Mapping

Sistem harus mengubah hasil deteksi app menjadi payload Discord Rich Presence.

Field umum:

- `details`
- `state`
- `largeImageKey`
- `largeImageText`
- `smallImageKey`
- `smallImageText`
- `startTimestamp`
- `buttons` opsional

Rules:

- Jangan update jika payload sama dengan sebelumnya.
- Jangan tampilkan filename/project jika privacy setting melarang.
- Gunakan asset image dari Discord Developer Portal sesuai `clientId`.

Acceptance Criteria:

- Payload valid dikirim ke Discord.
- Duplicate payload tidak dikirim berulang.
- Presence berubah saat active app berubah.

---

### FR-06 — App Registry

Sistem memiliki registry untuk daftar aplikasi dan cara match-nya.

Contoh entry:

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

- Registry bisa ditambah tanpa compile ulang untuk generic detector.
- Plugin khusus bisa override registry generic.

---

## 9. Non-Functional Requirements

### Performance

- CPU idle target: rendah, idealnya di bawah 1–3% pada perangkat normal.
- Polling active window: 1–3 detik atau event-driven jika OS mendukung.
- Discord update interval: minimal 10–15 detik kecuali ada perubahan penting.

### Reliability

- Auto-reconnect Discord IPC.
- Plugin isolation: error plugin tidak crash core.
- Logging structured untuk debug.

### Security & Privacy

- Tidak menggunakan Discord user token.
- Tidak membaca isi dokumen/audio/project kecuali plugin memang membutuhkan dan user mengizinkan.
- Semua data tetap lokal pada MVP.
- File/project name default disembunyikan untuk aplikasi sensitif.

### Maintainability

- Core tidak tahu detail aplikasi spesifik.
- App-specific logic berada di plugin.
- Unit test untuk matcher dan mapper.
- Integration test untuk Discord RPC connector dengan mock IPC.

---

## 10. Arsitektur Sistem

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

## 11. Komponen Teknis

### 11.1 Core Orchestrator

Tugas:

- Menjalankan lifecycle app.
- Menjadwalkan polling atau event subscription.
- Menyimpan current state.
- Membandingkan activity baru dengan activity lama.
- Mengatur debounce dan rate-limit.

State machine:

```text
STARTING -> DISCORD_CONNECTING -> READY -> DETECTING -> UPDATING_RPC
                             \-> DISCORD_UNAVAILABLE -> RETRYING
```

---

### 11.2 OS Integration Layer

Tugas:

- Ambil active window.
- Ambil process info.
- Ambil window title.
- Normalisasi data antar OS.

Output standar:

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

Tugas:

- Load plugin dari folder internal.
- Urutkan plugin berdasarkan priority.
- Jalankan `match()` untuk active snapshot.
- Jalankan `extract()` pada detector yang cocok.
- Fallback ke generic detector.

Priority contoh:

1. App-specific plugin dengan metadata detail.
2. Registry-based detector.
3. Generic process detector.

---

### 11.4 Discord RPC Adapter

Tugas:

- Connect ke Discord local IPC.
- Kirim `SET_ACTIVITY`.
- Clear activity saat shutdown.
- Reconnect otomatis.
- Handle error IPC.

Payload internal:

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

## 12. Pilihan Tech Stack

### Opsi Utama — C++ Native Desktop (Recommended sesuai requirement)

Karena target implementasi menggunakan C++ dan berjalan sebagai desktop app/background service, arsitektur akan difokuskan ke native layer.

Komponen:

- C++17/20 sebagai core language.
- OS-specific API untuk active window detection.
- Discord RPC via native IPC (pipe/socket) atau wrapper library.
- Optional lightweight GUI (tray) menggunakan Qt / Win32 / GTK.

Kelebihan:

- Performa sangat tinggi dan low memory.
- Kontrol penuh terhadap OS-level API.
- Cocok untuk background daemon/service jangka panjang.
- Tidak butuh runtime tambahan (seperti Node.js).

Kekurangan:

- Development lebih kompleks.
- Plugin system lebih sulit dibanding JS/TS.
- Cross-platform handling lebih banyak effort.

---

### Struktur Arsitektur (C++)

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

## 13. Library yang Dibutuhkan

### Core C++ Libraries

- `nlohmann/json` → parsing config JSON
- `spdlog` → logging cepat dan ringan
- `fmt` → formatting string modern
- `asio` atau `boost::asio` → async event loop (optional)
- `filesystem` (std) → file handling

---

### Discord RPC

Opsi:

1. **discord-rpc (official legacy)**
   - C-based library
   - Mudah dipakai di C++

2. **Custom IPC implementation (recommended advanced)**
   - Direct komunikasi ke Discord IPC pipe (`\?\pipe\discord-ipc-0` di Windows)
   - Lebih fleksibel dan future-proof

3. **Third-party C++ wrapper**
   - Wrapper di atas discord-rpc atau IPC

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
- Tools fallback:
  - `xprop`
  - `wmctrl`

#### Linux (Wayland)

- Sangat terbatas → fallback ke process-based detection

---

### Desktop UI (Optional)

- **Qt (recommended jika butuh UI)**
  - Tray icon
  - Settings window

- Alternatif:
  - Win32 tray API (Windows only)
  - GTK (Linux)

---

### Build System

- `CMake` (wajib)
- `vcpkg` atau `conan` untuk dependency management

---

### Testing

- `GoogleTest`
- `Catch2`

---

### Plugin System (C++)

Ada 2 pendekatan:

#### A. Static Plugin (Recommended MVP)

- Semua detector di-compile dalam binary
- Registry JSON untuk mapping
- Simpel dan stabil

#### B. Dynamic Plugin (Advanced)

- Load `.dll` / `.so`
- Interface via abstract class

```cpp
class AppDetector {
public:
    virtual bool match(const ActiveWindowSnapshot&) = 0;
    virtual ActivityPayload extract(const ActiveWindowSnapshot&) = 0;
};
```

- Lebih fleksibel tapi kompleks

---

### Utilities Tambahan

- `inih` atau JSON config loader ringan
- `chrono` untuk timing
- `thread` untuk scheduler/polling

---

## 14. App Support Strategy

### Level 1 — Generic Support

Support berdasarkan process/window title.

Cocok untuk:

- Okular
- Adobe apps
- FL Studio
- Ableton
- VS Code
- Browser
- Terminal

Output sederhana tapi stabil.

### Level 2 — Smart Window Title Parsing

Support berdasarkan pola title.

Contoh:

- Okular: ambil nama PDF dari window title.
- Photoshop: ambil nama file aktif dari title.
- Ableton/FL Studio: ambil nama project jika muncul di title.

### Level 3 — Deep Integration

Support melalui API/plugin khusus jika aplikasi menyediakan API, scripting, atau local state.

Contoh:

- DAW plugin/bridge untuk Ableton/FL Studio jika ingin BPM, playing/paused, project name.
- Adobe scripting/UXP/CEP bila butuh metadata dokumen lebih detail.

MVP cukup Level 1 + sebagian Level 2.

---

## 15. MVP Scope

### In Scope

- Windows + Linux X11 support.
- Discord RPC connect/reconnect.
- Active window detection.
- Plugin manager sederhana.
- Built-in detector untuk:
  - Okular
  - Ableton Live
  - FL Studio
  - Adobe Photoshop
  - Adobe Illustrator
  - Adobe Premiere Pro
  - Adobe After Effects
  - Generic app detector
- Config JSON.
- Privacy setting.
- Tray icon minimal.
- Logging file lokal.

### Out of Scope MVP

- Deep DAW metadata seperti BPM/track/playing state.
- Adobe UXP/CEP plugin.
- Cloud sync.
- Plugin marketplace.
- Mobile support.

---

## 16. Milestones

### M1 — Proof of Concept

Deliverables:

- Discord RPC berhasil tampil.
- Active app detection berjalan di satu OS.
- Hardcoded detector untuk 2 aplikasi.

### M2 — MVP Architecture

Deliverables:

- Plugin interface.
- Detector manager.
- Config manager.
- Activity mapper.
- Built-in detector awal.

### M3 — Desktop App

Deliverables:

- Tray app.
- Autostart option.
- Settings file.
- Logs viewer sederhana.

### M4 — App Expansion

Deliverables:

- Support Okular, Ableton, FL Studio, Adobe suite.
- Registry-based app definitions.
- Privacy controls.

### M5 — Stabilization

Deliverables:

- Reconnect handling.
- Rate-limit testing.
- Packaging installer.
- Documentation plugin authoring.

---

## 17. Success Metrics

- RPC muncul dengan benar untuk minimal 8 aplikasi target.
- App tetap berjalan 8 jam tanpa crash.
- CPU idle rendah.
- Discord reconnect berhasil setelah Discord restart.
- Menambah app baru lewat registry membutuhkan kurang dari 10 menit.
- 90% perubahan active app terdeteksi dalam 3–5 detik.

---

## 18. Risiko & Mitigasi

### Risiko: Wayland membatasi akses active window

Mitigasi:

- Fokus MVP pada Windows dan Linux X11.
- Sediakan fallback manual/app whitelist.
- Dokumentasikan keterbatasan Wayland.

### Risiko: Discord RPC unstable saat Discord restart

Mitigasi:

- Implement reconnect loop exponential backoff.
- Jangan crash saat IPC unavailable.

### Risiko: Privacy concern karena nama file tampil

Mitigasi:

- Default `showFileName = false`.
- Per-app privacy override.
- Preview payload sebelum dikirim.

### Risiko: Terlalu banyak plugin hardcoded

Mitigasi:

- Registry JSON untuk generic apps.
- Plugin hanya untuk app yang butuh parsing khusus.

---

## 19. Open Questions

1. Platform prioritas pertama: Windows, Linux, atau dua-duanya?
2. Apakah app harus punya UI penuh atau cukup tray + config file?
3. Apakah nama file/project boleh tampil secara default?
4. Apakah user butuh custom template per aplikasi?
5. Apakah ingin support Discord Web/custom client via bridge seperti arRPC, atau cukup Discord Desktop official?

---

## 20. Rekomendasi Implementasi Awal

Mulai dari struktur project berikut:

```text
rpc-presence/
  apps/
    desktop/              # Tauri app
  packages/
    core/                 # orchestrator, state machine
    discord-adapter/      # RPC connector
    os-detector/          # active window snapshot
    detectors/            # built-in plugins
    config/               # schema + loader
    registry/             # app definitions JSON
  configs/
    default.json
  docs/
    plugin-authoring.md
```

Prioritas coding:

1. Buat Discord RPC adapter.
2. Buat active window detector untuk OS utama.
3. Buat activity mapper.
4. Buat detector generic.
5. Tambah detector khusus untuk Okular, Ableton, FL Studio, dan Adobe.
6. Tambah tray + settings.
7. Tambah testing dan packaging.

---

## 21. Definition of Done MVP

MVP dianggap selesai jika:

- Aplikasi bisa dijalankan sebagai background/tray app.
- Discord status berubah sesuai active app.
- Minimal 8 app target dikenali.
- Config privacy berfungsi.
- App tidak crash saat Discord tidak berjalan.
- Log error dapat dibaca untuk debugging.
- Dokumentasi setup dan penambahan app baru tersedia.

---

## Ketentuan Teknis (C++20 + Modules)

Repo ini disiapkan untuk **C++20** dan **C++ Modules** (file `.cppm`).

### Struktur Folder

```text
include/                # Header publik (non-module) .hpp
modules/                # C++ module interface units .cppm (dipisah dari src/)
src/                    # Entry point + implementasi .cpp/.cc non-module
```

### Aturan Modules vs Include

- Internal codebase **utamakan `import`** untuk dependency antar-komponen.
  - Contoh: `import rpc.core;`
- `#include` tetap dipakai untuk:
  - header standard / third-party
  - header non-module di `include/` bila memang diperlukan
- Untuk `.cppm`, jika perlu `#include`, taruh di **global module fragment**:
  - pola: `module;` → `#include ...` → `export module ...;`

### Build & Preset

- Tooling minimal: **CMake >= 3.28** + **Ninja** + compiler C++20 yang mendukung modules.
- Quick start (Windows/Clang):
  - `cmake --preset clang-release`
  - `cmake --build --preset clang-release`

### Optimisasi

- Default Release preset mengaktifkan LTO via option `SOFTWARE_RPC_ENABLE_LTO=ON`.
- Optional native CPU optim (tidak portable): `-DSOFTWARE_RPC_ENABLE_NATIVE_OPT=ON` (GCC/Clang).
