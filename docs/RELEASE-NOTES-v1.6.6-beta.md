# NLSI Exclusive Logbook 1.6.6 Beta

## Release contents

- Windows x64 Qt Widgets dashboard, with embedded product metadata for
  version 1.6.6 Beta.
- SCS Telemetry SDK 1.15 plugin builds for both x64 and x86 game processes.
- Qt runtime and platform-plugin dependencies deployed beside the dashboard.
- Local, read-only telemetry over the existing Windows named pipe. The
  dashboard continues to show unavailable telemetry as an em dash and does
  not add remote synchronization or persistent telemetry storage.
- Installer support for new installations and in-place upgrades using the
  stable NLSI product identifier.

## Plugin installation and safety

The installer offers optional ETS2 and ATS plugin installation when it finds a
game executable in a default Steam location or a Steam library registered in
`libraryfolders.vdf`. It installs only the matching x64 or x86 DLL into an
existing game architecture directory and creates that architecture's
`plugins` directory only after the user selects the corresponding task.

An existing plugin is replaced only when its NLSI ownership marker is
recognized and, for 1.6.6-managed plugins, the recorded SHA-256 still matches.
An existing unmarked or changed DLL is left untouched and reported as skipped.
Before replacing a marked older NLSI DLL, the installer retains a
`.nlsi-backup.dll` copy. Uninstall restores that backup only if the current
plugin still matches the installer-written hash; a user-modified file is left
untouched and its backup is retained.

## Upgrade compatibility

The stable installer identifier supports in-place updates from the existing
1.6.5 Beta installer while preserving its registered installation location.
Older releases that used a different installer identifier require removal
through their own uninstaller first. User-owned files and unrelated files in
the installation and game directories are not targeted for cleanup.

## Validation

The release build script configures and builds the x64 dashboard and plugin,
builds the x86 plugin separately, runs all configured automated tests,
inspects PE machine types and SCS exports, deploys and checks the Qt runtime,
smoke-launches the packaged dashboard, compiles the installer, and writes a
SHA-256 manifest. The manifest and the installation guide accompany the
installer.

The installer was compiled and its metadata inspected, but actual
fresh-install, 1.6.5-to-1.6.6 update, uninstall, and game-plugin installation
execution tests were not run. The available Windows session is not elevated,
and the detected ETS2 plugin paths already contain DLLs; performing those
tests would require administrator approval and could modify existing game
plugins. Those scenarios are supported by the installer logic but are not
claimed as tested.
