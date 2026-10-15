# NLSI Exclusive Logbook 1.6.7 Beta

## Release contents

- Windows x64 Qt Widgets dashboard with product version `1.6.7 Beta`.
- SCS Telemetry SDK 1.15 plugins built for x64 and x86 game processes.
- Qt runtime, Qt platform plugins, application resources, and app-local MSVC
  runtime files.
- Read-only local telemetry; no network uploads or telemetry persistence.
- An x64 installer with a stable product identifier for in-place updates.

## Plugin installation

The installer discovers ETS2 and ATS in the default Steam library and
registered Steam libraries. When a game and a matching architecture directory
are found, the installer offers the corresponding plugin task, selected by
default. Each task installs only the matching DLL architecture. Missing game
directories do not block the desktop application installation.

An existing game plugin is replaced only when its NLSI ownership marker is
recognized and its recorded hash matches. Unmarked or changed DLLs are left
untouched to protect unrelated or user-modified plugins. An existing NLSI
plugin is backed up before replacement. On uninstall, an unmodified plugin
installed by this product is removed or restored from its backup.

## Updating

The stable installer identifier detects an existing installation and retains
its registered install directory when that directory still exists. Running
the installer again updates the application in place; a manual uninstall is
not required. Installer-owned binaries and runtime files present in the new
payload are replaced. User-created files are not targeted for cleanup.

## Validation record

The release helper completed fresh x64 dashboard/plugin and x86 plugin Release
builds using MSVC 19.44.35229.0. The telemetry IPC and dashboard UI test suite
passed (2/2). The helper inspected PE machine types, verified
`scs_telemetry_init` and `scs_telemetry_shutdown` exports in both plugin
architectures, checked the app version resource, deployed Qt and app-local
runtime dependencies, and smoke-launched the staged dashboard.

The installer was compiled with the official Inno Setup 6.7.3 compiler.
Compilation parsed the installer code and compressed the dashboard, Qt runtime,
product icon/resources, and both architecture-specific plugin DLLs into the
versioned setup executable. The release helper checked installer size and
version metadata and retained the staged payload; SHA-256 values for the
installer, dashboard executable, both plugin DLLs, and release documents are
recorded in `manifest.sha256`.

Fresh-install, in-place update, plugin installation into actual game
directories, and uninstall execution were not run. No installer was launched
or live application/game installation modified during this validation.
