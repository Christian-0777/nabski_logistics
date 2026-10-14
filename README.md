# NLSI Exclusive Logbook

NLSI Exclusive Logbook is a Windows desktop application for telemetry from **Euro Truck Simulator 2 (ETS2)** and **American Truck Simulator (ATS)**. The native desktop application is developed in C++ with Qt Widgets and uses the SCS Telemetry SDK for supported telemetry channels.

> **Project status:** Beta / active development. Features and live gameplay behavior may change between releases.

## Features

- Read-only dashboard for supported truck, driving, job, navigation, and vehicle configuration telemetry.
- Automatic ETS2 / ATS process detection.
- Native Windows desktop interface built with C++ and Qt Widgets.
- Separate SCS telemetry plugin DLLs for supported Windows architectures.
- Explicit unavailable states for telemetry fields that are unsupported or have no valid data.
- Local operation without requiring a database or remote server for the read-only dashboard.

Available fields depend on the game, installed plugin, SDK channel support, and whether a valid current sample is provided. The application should not treat missing data as a real value or infer unsupported telemetry.

## Supported Games

- Euro Truck Simulator 2 (ETS2)
- American Truck Simulator (ATS)

## Technology

- C++ and Qt Widgets
- CMake
- Official SCS Telemetry SDK 1.15 headers
- Windows x64 desktop application
- Architecture-matched x64 and x86 telemetry plugin DLLs where supported

## Build Requirements

Use a compatible Windows C++ toolchain and Qt installation:

- Microsoft Visual Studio / Build Tools with MSVC and a Windows SDK
- CMake
- Ninja (if selected as the CMake generator)
- Qt 6 Widgets built for the same compiler ABI
- The SCS Telemetry SDK headers included with this project

Do not mix MSVC-built Qt libraries with MinGW toolchains.

## Build

Open a Visual Studio Developer Command Prompt or initialize the MSVC environment, then run from the project root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON -DBUILD_DASHBOARD=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The exact CMake options and target names depend on the checked-out project configuration. Inspect `CMakeLists.txt` before using optional flags. Configure a separate Win32 build directory if an x86 plugin is required; do not copy a DLL between architectures.

## Installation

Download installers from the repository's [GitHub Releases](https://github.com/Christian-0777/nlsi_telemetry/releases) page when a release asset is available. Follow the release-specific installation instructions. Installing game plugin DLLs may require administrator privileges and should only replace NLSI-owned files.

The project is under active development. Do not assume every source field, plugin combination, installer path, or live gameplay scenario has been tested for every beta version.

## Development Notes

- Keep telemetry values read-only unless a future feature is explicitly designed and documented.
- Do not add remote networking, database access, or persistence to a read-only dashboard without an approved requirement.
- Confirm channel names, units, callbacks, and value lifetimes against the exact SCS SDK headers.
- Do not disable, remove, rename, or modify third-party telemetry plugins as a troubleshooting shortcut.
- Keep local build output, generated binaries, credentials, dumps, and user data out of source control.
- Plugin binaries and SDK components may be subject to their applicable licenses. Review those licenses before redistribution.

## License

No project-wide license is declared by this README. Check the license and redistribution terms of individual components and dependencies before copying or distributing them.
