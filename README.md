# Discord Resistance Software — Technical Documentation

Welcome to the technical documentation for **Discord Resistance Software**. This document provides an in-depth overview of the architecture, module structure, and development guidelines for this project.

---

## 🏗️ System Architecture

This application is built using **C++20** with a focus on high performance and low memory footprint. The architecture is modular, separating core logic, resources, and the user interface.

### Main Modules
- **`rpc_core`**: Handles active window detection logic, application state management, and Rich Presence orchestration.
- **`rpc_res`**: Serves as a resource library (DLL) that stores image assets and icons required by the application.
- **`rpc_config`**: A configuration module that handles reading settings from `.env` files (during debug) or constant values (during release).

---

## 📂 Directory Structure

- **`src/`**: Contains the main application source code (entry point, tray implementation, win32 specifics).
- **`include/`**: Public header files for various system components.
- **`modules/`**: C++20 Module implementations for core logic.
- **`res/`**: Graphical assets (icons, bitmaps) to be compiled into `rpc_res.dll`.
- **`cmake/`**: CMake helper scripts for dependency management and packaging.

---

## 🛠️ Development Guide

### Prerequisites
- **Compiler**: MSVC (supporting C++20 Modules).
- **Build System**: CMake 3.20+.
- **Dependencies**: 
  - `nlohmann_json` (JSON Parsing)
  - `spdlog` (Logging)
  - `Discord Game SDK / RPC Library`

### Build Configurations
1. **Debug**: Uses `.env` files for flexible configuration and enables the debugging console (`AllocConsole`).
2. **Release**: Uses hardcoded configurations for security and performance. The console is disabled, and the application runs purely in the background/tray.

---

## 🔌 Application Detection (Detectors)

The detection system works by monitoring the foreground window using the Win32 API. Each supported application has a matching pattern based on:
1. **Process Name** (e.g., `Ableton Live 11 Suite.exe`)
2. **Window Title** (e.g., "FL Studio" substring)

This logic can be extended via `modules/rpc/core/orchestrator.cppm`.

---

## 📜 License

This project is licensed under the **GNU GPL v3**. See the [LICENSE](file:///c:/Users/Administrator/Documents/Project/software-discord-rpc/LICENSE) file for more details.

---

*Last updated: May 8, 2026*
