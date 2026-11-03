# NLSI Exclusive Logbook v1.9.5 Beta Installation

When a v1.9.5 Beta installer is published, run
`NLSI-Exclusive-Logbook-v1.9.5-beta-Setup.exe` and follow the installer. It
retains the existing Inno Setup AppId and recognizes existing NLSI
installations, including v1.9.4 Beta.

The installer manages application, Qt runtime, image-format, and telemetry
plugin files. It does not manage per-user preferences, encrypted API
credentials, completed-job history, synchronization state, or local `.nlsi`
records. Existing unmanaged or modified game plugin DLLs are not overwritten.

## Release output paths

Relative to the desktop project root:

- Installer:
  `releases\1.9.5-beta\NLSI-Exclusive-Logbook-v1.9.5-beta-Setup.exe`
- Dashboard:
  `releases\1.9.5-beta\payload\nlsi_dashboard.exe`
- x64 and x86 SCS plugins:
  `releases\1.9.5-beta\payload\plugin\win_x64\nlsi_scs_plugin.dll` and
  `releases\1.9.5-beta\payload\plugin\win_x86\nlsi_scs_plugin.dll`
- Qt WebP image-format plugin:
  `releases\1.9.5-beta\payload\imageformats\qwebp.dll`
- SHA-256 manifest:
  `releases\1.9.5-beta\manifest.sha256`

Close the dashboard and ETS2/ATS before installing or upgrading so that
application and game-plugin files are not locked. The dashboard prevents
startup and shutdown while a supported game is running.

This release remains **Beta**. The installer asset is not considered published
until it appears on the official GitHub Releases page.
