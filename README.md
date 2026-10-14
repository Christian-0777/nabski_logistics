# NLSI Exclusive Logbook

NLSI Exclusive Logbook is a native Windows desktop telemetry dashboard for **Euro Truck Simulator 2 (ETS2)** and **American Truck Simulator (ATS)**. The application is developed in C++17 with Qt Widgets and interfaces with the official SCS Telemetry SDK (version 1.15) to display real-time vehicle, driving, navigation, and job metrics.

> **Project Status:** Beta / Active Development (current release: `v1.6.9 Beta`). Features, telemetry mappings, and live gameplay behavior may evolve between releases. Unverified gameplay scenarios or unconfirmed SDK channels are never assumed or fabricated.

---

## Key Features

- **Real-Time Read-Only Telemetry:** Displays truck status, vehicle speed, engine RPM, gear selection, fuel levels, cruise control, retarder engagement, and applied brake pedal indicators.
- **Automatic Game Process Detection:** Identifies running instances of Euro Truck Simulator 2 (`eurotrucks2.exe`) and American Truck Simulator (`amtrucks.exe`).
- **Native C++/Qt Architecture:** High-performance desktop UI built with C++17 and Qt 6 Widgets (with Qt 5 fallback in CMake).
- **Architecture-Matched Telemetry Plugins:** Dedicated x64 and x86 SCS telemetry plugin DLLs (`nlsi_scs_plugin.dll`) linking statically against the MSVC runtime.
- **Local Inter-Process Communication (IPC):** Communicates securely between game plugin and dashboard over a local Windows named pipe (`\\.\pipe\nlsi_scs_telemetry`) via `QLocalServer`/`QLocalSocket`.
- **Explicit Unavailable States:** Unsupported or missing telemetry channels are rendered as explicit em dashes (`—`) rather than defaulting to zero or fabricating inferred values.
- **Fuel Warning & Precision:** RPM and fuel are formatted to two decimal places, with visual warning highlights when fuel drops to or below 20% of tank capacity.
- **Navigation Advisor ETA Conversion:** Converts SDK navigation advisor seconds to real-time duration (using a 15:1 game-to-real-time scale) with local arrival clock and day rollover estimation.
- **No Remote Dependencies:** Completely self-contained local operation; requires no external web services, background cloud daemons, or database servers for telemetry monitoring.

---

## Supported Games

- **Euro Truck Simulator 2 (ETS2)** (x64 and x86)
- **American Truck Simulator (ATS)** (x64 and x86)

---

## Architecture & Technology Stack

| Component | Technology | Description |
|---|---|---|
| **Dashboard UI** | C++17 / Qt Widgets (Qt 6.x or Qt 5.x) | Native Windows GUI application (`nlsi_dashboard.exe`). |
| **Telemetry Plugin** | C++17 / SCS SDK 1.15 | DLL (`nlsi_scs_plugin.dll`) loaded by the game engine. |
| **IPC Layer** | Windows Named Pipe (`\\.\pipe\nlsi_scs_telemetry`) | Fast, local-only frame transmission between plugin and dashboard. |
| **Build System** | CMake 3.16+ / Ninja / MSVC 2022 | Cross-architecture build configuration supporting x64 and Win32. |
| **Validation** | CTest (`nlsi_telemetry_tests`, `nlsi_dashboard_ui_tests`) | Unit and UI regression tests (supporting offscreen Qt platform). |
| **Packaging** | Inno Setup 6.7+ / PowerShell | Automated installer packaging and Steam plugin deployment. |

---

## Build Prerequisites

To build NLSI Exclusive Logbook from source, ensure the following tools are installed:

- **Operating System:** Windows 10 or Windows 11 (64-bit)
- **Compiler:** Microsoft Visual Studio 2022 (or Visual Studio Build Tools 2022) with the **Desktop development with C++** workload and Windows SDK (10.0.26100.0 or compatible)
- **CMake:** CMake 3.16 or higher (CMake 3.24+ recommended for `--fresh` rebuilds)
- **Generator:** Ninja (bundled with Visual Studio) or Visual Studio 17 2022 generator
- **Qt Framework:** Qt 6.12.0+ (or Qt 5.15+) MSVC 2022 64-bit with `Core`, `Widgets`, `Network`, and `Concurrent` modules
- **SCS Telemetry SDK:** Version 1.15 headers located in `scs_sdk_1_15/include`
- **Installer Tooling (Optional):** Inno Setup 6.7+ for building the production installer

> [!IMPORTANT]
> Always match compiler toolchains: do not mix MinGW-compiled Qt libraries with MSVC binaries. The 32-bit plugin requires a separate Win32 MSVC configuration without 64-bit Qt linkage.

---

## Build Instructions

### 1. Configure Visual Studio Environment

Open a standard Command Prompt and initialize the x64 Developer Command Prompt:

```cmd
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
```

### 2. Configure and Build (x64 Dashboard & Plugin)

Set your Qt installation path in `CMAKE_PREFIX_PATH` and compile:

```cmd
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.12.0\msvc2022_64" -DBUILD_TESTING=ON -DBUILD_DASHBOARD=ON
cmake --build build --config Release
```

Alternatively, use the Visual Studio 2022 generator:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON -DBUILD_DASHBOARD=ON
cmake --build build --config Release
```

### 3. Run Automated Tests

Execute the CTest test suite to validate IPC serialization, process classification, and offscreen UI behavior:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

### 4. Build 32-bit (x86) Telemetry Plugin

For 32-bit game compatibility, build the plugin separately with `BUILD_DASHBOARD=OFF`:

```powershell
cmake -S . -B build-x86 -G "Visual Studio 17 2022" -A Win32 -DBUILD_DASHBOARD=OFF -DBUILD_TESTING=OFF
cmake --build build-x86 --config Release
```

### 5. Automated Clean Build and Release Packaging

- To clean and rebuild the default workspace:
  ```bat
  scripts\clean-build.bat
  ```
- To run the automated release pipeline (x64 build, test run, x86 build, staging, Qt deployment, and Inno Setup installer compilation):
  ```powershell
  powershell -ExecutionPolicy Bypass -File .\scripts\build-release.ps1
  ```

---

## Installation & Deployment

1. Download the latest installer (`NLSI-Exclusive-Logbook-v1.6.9-beta-Setup.exe`) from the [GitHub Releases](https://github.com/Christian-0777/nlsi_telemetry/releases) page.
2. Run the installer and select your installation directory (defaults to `C:\Program Files\Nabski Logistics Solutions Inc`).
3. The installer scans known Steam library configurations (`libraryfolders.vdf`) to detect ETS2 and ATS installations.
4. If enabled during setup, the installer deploys `nlsi_scs_plugin.dll` to the architecture-specific `bin\win_x64\plugins` and `bin\win_x86\plugins` game folders.
5. Unmanaged or user-modified third-party plugins are never overwritten. Existing NLSI-managed plugins are safely backed up before replacement.

---

## Development & Safety Notes

- **Read-Only Telemetry:** Telemetry data is strictly read-only. No telemetry injection, game memory patching, or input manipulation is performed.
- **Defensive Error Handling:** If a telemetry field is missing, stale, or unsupported by the current game version, it is displayed as unavailable rather than estimated.
- **Third-Party Plugins:** Do not delete or overwrite third-party telemetry DLLs when installing or testing.
- **Local Git Hygiene:** Local build output (`build/`, `build-x86/`), binaries (`*.exe`, `*.dll`, `*.lib`), release payloads (`releases/*/payload/`), secrets (`.env`, `credentials.json`), and temporary crash dumps (`*.dmp`, `*.log`) are kept out of version control.

---

## License & Credits

- Telemetry interfaces utilize SCS Software's official SCS Telemetry SDK (version 1.15).
- Application UI components are developed with the Qt framework under applicable Qt licensing terms.
- Refer to individual licenses in `scs_sdk_1_15/sdk_license.txt` and project component notices before redistribution.
