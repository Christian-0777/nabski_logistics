# NLSI Exclusive Logbook v1.9.2 Beta Installation

Run `NLSI-Exclusive-Logbook-v1.9.2-beta-Setup.exe` and follow the installer.
It supports fresh installation and upgrades an existing NLSI Logbook install
using the stable Inno Setup AppId and the previous install location.

The installer updates managed application, Qt runtime, image-format, and
telemetry-plugin files. It does not copy, replace, or remove per-user
preferences, encrypted API credentials, completed-job history, or local
`.nlsi` records. Existing unmanaged or modified game plugin DLLs are not
overwritten.

## Release output paths

Relative to the desktop project root:

- Installer:
  `releases\1.9.2-beta\NLSI-Exclusive-Logbook-v1.9.2-beta-Setup.exe`
- Packaged application:
  `releases\1.9.2-beta\payload\nlsi_dashboard.exe`
- WebP image plugin:
  `releases\1.9.2-beta\payload\imageformats\qwebp.dll`
- x64 and x86 SCS plugins:
  `releases\1.9.2-beta\payload\plugin\win_x64\nlsi_scs_plugin.dll` and
  `releases\1.9.2-beta\payload\plugin\win_x86\nlsi_scs_plugin.dll`

Close ETS2 and ATS before installing optional telemetry plugins. The desktop
also prevents normal startup and window closure while either game process is
running. The installer installs only matching plugin architectures into
detected game plugin directories and preserves existing user data.

No data migration is required. Settings, API credentials, local records, and
synchronization state remain in their existing per-user application-data
locations.

This release remains **Beta**.
