# NLSI Exclusive Logbook

> GitHub repository: `nabski_logistics`.

NLSI Exclusive Logbook is a native Windows desktop telemetry dashboard for **Euro Truck Simulator 2 (ETS2)** and **American Truck Simulator (ATS)**. The application is developed in C++17 with Qt Widgets and interfaces with the official SCS Telemetry SDK (version 1.15) to display real-time vehicle, driving, navigation, and job metrics.

> **Project Status:** Beta / Active Development (current release: `v1.7.4 Beta`). Features, telemetry mappings, and live gameplay behavior may evolve between releases. Unverified gameplay scenarios or unconfirmed SDK channels are never assumed or fabricated.

---

## Install or Update

For v1.7.4, install over the existing version without uninstalling; the
installer updates the application and automatically replaces eligible
NLSI-managed plugin DLLs. See the
[current installation guide](docs/INSTALLATION-v1.7.4-beta.md).

1. **Back up records you want to keep.** Copy any important logs, job/session history, exports, and configuration from the existing installation or its user-data directory. The usual per-user data location is `%LOCALAPPDATA%\NLSI\Exclusive Logbook`; verify what your installed build actually uses before removing anything.
2. **Close the application and games.** Exit NLSI Exclusive Logbook. Close ETS2 and ATS completely and check Task Manager for any remaining application/game processes.
3. **Update in place.** Run the v1.7.4 installer over the existing application. Manual uninstall is not required.
4. **Close games before setup.** If a game DLL is locked, close the game or other process using it and retry. Do not manually delete unrelated game files or third-party telemetry plugins.
5. **Install the update.** Grant administrator permission if Windows requests it for protected application or game plugin folders.
6. **Verify plugin deployment.** The installer automatically deploys the matching x64/x86 DLLs to detected game directories. Only unchanged NLSI-managed plugin files are replaced.
7. **Test before relying on it.** Launch NLSI first, then start one game. Confirm process detection and telemetry connection, try a short session, and inspect the application/game logs if it crashes. Test the other game separately.
8. **Keep user records.** Job history and the job-ID sequence remain in the user's application-data directory during an in-place update.

**Important:** This release remains Beta. Until ETS2 and ATS gameplay scenarios have been checked on your installation, treat live-game behavior as unverified.

---

## Key Features

- **Real-Time Read-Only Telemetry:** Displays truck status, vehicle speed, engine RPM, gear selection, fuel levels, cruise control, retarder engagement, and applied brake pedal indicators.
- **Automatic Game Process Detection:** Identifies running instances of Euro Truck Simulator 2 (`eurotrucks2.exe`) and American Truck Simulator (`amtrucks.exe`).
- **Native C++/Qt Architecture:** High-performance desktop UI built with C++17 and Qt 6 Widgets (with Qt 5 fallback in CMake).
- **Architecture-Matched Telemetry Plugins:** Dedicated x64 and x86 SCS telemetry plugin DLLs (`nlsi_scs_plugin.dll`) linking statically against the MSVC runtime.
- **Local Inter-Process Communication (IPC):** Communicates securely between game plugin and dashboard over a local Windows named pipe (`\\.\pipe\nlsi_scs_telemetry`) via `QLocalServer`/`QLocalSocket`.
- **Explicit Unavailable States:** Unsupported or missing telemetry channels are rendered as explicit em dashes (`—`) rather than defaulting to zero or fabricating inferred values.
- **Fuel Warning & Precision:** RPM and fuel are formatted to two decimal places, with visual warning highlights when fuel drops to or below 20% of tank capacity.
- **Fuel Cost Availability:** Fuel cost stays unavailable until telemetry provides a reliable refueling price and quantity; refueled liters and total fuel consumed remain separate metrics.
- **Navigation Advisor ETA Conversion:** Converts SDK navigation advisor seconds to real-time duration (using a 15:1 game-to-real-time scale) with local arrival clock and day rollover estimation.
- **No Remote Dependencies:** Completely self-contained local operation; requires no external web services, background cloud daemons, or database servers for telemetry monitoring.
- **Persistent Job History:** Application-generated job IDs and confirmed delivered or cancelled jobs are stored as atomic local `.nlsi` records. Active jobs recover across dashboard restarts and remain excluded from completed history.
- **Completed Jobs:** A dedicated page lists only confirmed terminal jobs, newest first, and opens a complete details view for each record.

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

1. Download the v1.7.4 Beta installer (`NLSI-Exclusive-Logbook-v1.7.4-beta-Setup.exe`) from the [GitHub Releases](https://github.com/Christian-0777/nlsi_telemetry/releases) page.
2. Run the installer and select your installation directory (defaults to `C:\Program Files\Nabski Logistics Solutions Inc`).
3. The installer scans known Steam library configurations (`libraryfolders.vdf`) to detect ETS2 and ATS installations.
4. The installer automatically deploys `nlsi_scs_plugin.dll` to detected architecture-specific `bin\win_x64\plugins` and `bin\win_x86\plugins` game folders.
5. Unmanaged or user-modified third-party plugins are never overwritten. Existing NLSI-managed plugins are safely backed up before replacement.
6. If ETS2 or ATS is running, Setup blocks before replacing files and asks you to close the game; it never terminates either game. Job records and the ID sequence remain under the user's application-data directory and are preserved by updates and uninstall.

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
