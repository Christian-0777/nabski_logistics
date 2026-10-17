# NLSI Exclusive Logbook v1.8.8 Beta Installation

Run `NLSI-Exclusive-Logbook-v1.8.8-beta-Setup.exe` from the release output
directory and follow the installer. It supports upgrading an existing NLSI
Logbook installation using the existing stable Inno Setup AppId and prior
install location. The installer does not copy, replace, or remove per-user
preferences, encrypted API credentials, or local job history.

The desktop application and Qt/MSVC runtime dependencies are installed
together. The installer contains both architecture-specific SCS plugin
payloads and installs a matching DLL only into a detected ETS2/ATS game
architecture directory. Existing unmanaged or modified game plugins are not
overwritten. Close ETS2 and ATS before installing plugins.

## Release output paths

Relative to the desktop project root:

- Installer:
  `releases\1.8.8-beta\NLSI-Exclusive-Logbook-v1.8.8-beta-Setup.exe`
- Packaged x64 application:
  `releases\1.8.8-beta\payload\nlsi_dashboard.exe`
- x64 SCS plugin:
  `releases\1.8.8-beta\payload\plugin\win_x64\nlsi_scs_plugin.dll`
- x86 SCS plugin:
  `releases\1.8.8-beta\payload\plugin\win_x86\nlsi_scs_plugin.dll`
- Qt/MSVC runtime dependencies and resources: `releases\1.8.8-beta\payload\`
- SHA-256 manifest:
  `releases\1.8.8-beta\manifest.sha256`

Verify the installer and packaged binaries using the hashes in
`manifest.sha256`. The release pipeline builds a clean version-specific
payload and validates its executable/DLL versions and architecture before
passing it to Inno Setup.

## Task data and database

No migration is included or required. Profile TASK reads the signed-in
account's existing website `monthly_tasks` rows through the authenticated API.
The API is read-only: it does not create a current-month task or change history.
If a new month has no initialized row, the desktop states this and shows any
stored history. Task progress requires an API connection and is not cached
offline.

## Validation results

- CTest Release: 5/5 passed.
- Website PHP tests: 8/8 passed.
- Shared API PHP tests: 4/4 passed.
- The release build additionally checks the x64 application, x64/x86 plugin
  architectures and exports, packaged payload versions, runtime dependencies,
  and installer version metadata.

This is a **Beta** release.
