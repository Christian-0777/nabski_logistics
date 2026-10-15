# Build guide

## Prerequisites

- Windows 10 or 11
- CMake 3.16+ for the project; CMake 3.24+ for the release helper's `--fresh` builds
- Qt 6.12.0 or later with the `Widgets` and `Network` modules installed
- Microsoft Visual Studio 2022 Build Tools with the MSVC x64 C++ workload and Windows SDK
- Ninja
- The SCS telemetry SDK headers in `scs_sdk_1_15/`

The verified build environment for this project used:

- Visual Studio Build Tools 2022, MSVC 19.44.35229, Windows SDK 10.0.26100.0
- CMake 4.4.4
- Ninja 1.12.1 (bundled with Visual Studio)
- Qt 6.12.0 MSVC 2022 x64, including Widgets and Network
- Inno Setup 6.7.3 (for the versioned installer build)

## Configure and build

Open a normal Command Prompt and initialize the x64 Visual Studio toolchain:

```cmd
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
```

Then configure and compile from the project root:

```cmd
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;C:\Qt\6.12.0\msvc2022_64\bin;%PATH%"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" -DCMAKE_PREFIX_PATH="C:\Qt\6.12.0\msvc2022_64" -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## Clean build helper

```bat
scripts\clean-build.bat
```

## Release installer

The versioned Windows release pipeline performs a fresh x64 MSVC Release build,
runs the telemetry tests, separately builds the x86 SCS plugin, validates both
plugin architectures and exports, stages the dashboard and both plugins,
deploys Qt and app-local MSVC runtime dependencies, and compiles the Inno Setup
installer. The release helper records SHA-256 hashes for the installer,
dashboard, both plugin DLLs, and release documents:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-release.ps1
```

The v1.7.4 Beta installer is written to
`releases\1.7.4-beta\NLSI-Exclusive-Logbook-v1.7.4-beta-Setup.exe` and
defaults to `C:\Program Files\Nabski Logistics Solutions Inc`. It uses the
stable Inno Setup AppId for updates, reuses a valid previous install location,
closes the dashboard through Windows Restart Manager, and leaves files not
owned by the installer alone.

The dashboard and Qt runtime are installed at the destination root. The x64
and x86 plugins are included at `plugin\win_x64\nlsi_scs_plugin.dll` and
`plugin\win_x86\nlsi_scs_plugin.dll`. The installer automatically deploys ETS2 and ATS plugins when an installed
executable is found in the default Steam
library or a Steam library recorded in `libraryfolders.vdf`. It copies each
architecture only to an existing, matching game architecture directory.
Existing unmarked or hash-changed DLLs are not overwritten. Marked plugins are
backed up before replacement and safely restored on uninstall when unchanged.

A plugin-only `BUILD_DASHBOARD=OFF` CMake configuration is used for x86 so that
the 32-bit DLL does not link against the dashboard's x64 Qt libraries.
Both plugin DLLs use the static MSVC runtime and are checked for imports,
architecture, and the required SCS exports before packaging.

The release package includes runtime assets and release documentation; the
installer and documentation SHA-256 values are written to `manifest.sha256`.
The final installer, staged payload, release documents, and SHA-256 manifest
are kept under `releases\1.7.4-beta`.

The plugin communicates with the dashboard over the local Windows named pipe `\\.\pipe\nlsi_scs_telemetry`. The dashboard listens through Qt LocalServer. Completed job history, the active recovery record, and the monotonic ID sequence are stored locally in the user's application-data folder.

## Targets

- `nlsi_dashboard` — Qt Widgets dashboard executable
- `nlsi_scs_plugin` — Windows DLL plugin built against the SCS SDK headers
- `nlsi_telemetry_tests` — IPC parsing/escaping, unavailable values, malformed
  frames, process-state classification, and telemetry connection states
- `nlsi_job_history_tests` — ID allocation, active-job restart recovery,
  terminal transitions, atomic records, duplicate events, sequence expansion,
  and malformed-record isolation
- `nlsi_preferences_tests` — exact conversions, encrypted preference
  persistence, corrupt-profile handling, and preservation of unrelated data
- `nlsi_dashboard_ui_tests` — dashboard settings and measurement presentation,
  completed-job navigation/cards/details, PDF generation, and vertical scrolling
  (runs with Qt's offscreen platform)

Preferences live in the per-user encrypted `profile.nlsi` under Qt's
application-data location. The installer only manages files in its application
payload and does not package or remove this profile.
