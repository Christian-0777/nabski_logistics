# NLSI Exclusive Logbook 1.6.8 Beta

## Release contents

- Windows x64 Qt Widgets dashboard with product version `1.6.8 Beta`.
- SCS Telemetry SDK 1.15 plugins built for x64 and x86 game processes.
- Qt runtime, Qt platform plugins, application resources, and app-local MSVC
  runtime files.
- Read-only local telemetry; no network uploads or telemetry persistence.
- An x64 installer with a stable product identifier for in-place updates.

## Dashboard

- Expanded the sidebar and kept the active Dashboard item full-width.
- Centralized the requested dashboard typography sizes for future releases.
- Standardized dashboard cards on white backgrounds with pink borders.
- Replaced the graphical progress bar with the existing percentage display.
- Shortened the arrival and fuel labels without changing their displayed values.
- Identified the active game in the Game Config heading and aligned the
  vehicle, vehicle plate, trailer, and trailer plate fields into two rows.
- Used the existing `resources\nlsi.ico` as the application icon.

## Plugin installation and updates

The installer discovers ETS2 and ATS in the default Steam library and
registered Steam libraries. When a game and matching architecture directory
are found, it offers the corresponding plugin task, selected by default. Each
task installs only the matching DLL architecture. Missing game directories do
not block the desktop application installation.

Recognized, unchanged NLSI plugins are eligible for replacement, including
plugins installed by previous releases. Unmarked or modified DLLs are left
untouched to protect unrelated or user-modified plugins.

The stable installer identifier detects an existing installation and retains
its registered install directory when that directory still exists. Application
files supplied in the new payload replace their prior versions; user-created
files are not targeted for deletion.
