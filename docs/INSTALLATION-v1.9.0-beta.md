# NLSI Exclusive Logbook v1.9.0 Beta Installation

Run `NLSI-Exclusive-Logbook-v1.9.0-beta-Setup.exe` and follow the installer.
It supports fresh installation and upgrades an existing NLSI Logbook install
using the stable Inno Setup AppId and prior install location.

The installer updates only its version-managed application payload. It does
not copy, replace, or remove per-user preferences, encrypted API credentials,
completed-job history, or local `.nlsi` records. Existing unmanaged or
modified game plugin DLLs are not overwritten.

## Release output paths

Relative to the desktop project root:

- Installer:
  `releases\1.9.0-beta\NLSI-Exclusive-Logbook-v1.9.0-beta-Setup.exe`
- Packaged x64 application:
  `releases\1.9.0-beta\payload\nlsi_dashboard.exe`
- x64 SCS plugin:
  `releases\1.9.0-beta\payload\plugin\win_x64\nlsi_scs_plugin.dll`
- x86 SCS plugin:
  `releases\1.9.0-beta\payload\plugin\win_x86\nlsi_scs_plugin.dll`
- Qt/MSVC runtime and product resources:
  `releases\1.9.0-beta\payload\`
- SHA-256 manifest: `releases\1.9.0-beta\manifest.sha256`

Close ETS2 and ATS before installing optional plugins. The installer installs
only matching plugin architectures into detected game plugin directories and
backs up an existing NLSI-managed plugin before replacing it.

No database migration is required. The desktop continues to store settings,
API credentials, local records, and synchronization state in the existing
per-user application-data locations.

This release remains **Beta**.
