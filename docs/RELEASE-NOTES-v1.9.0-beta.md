# NLSI Exclusive Logbook v1.9.0 Beta Release Notes

## Changes

- Removed the duplicate Profile account-details card. The profile header keeps
  the display name, username, role, and account status.
- Added authenticated loading of each account's uploaded website profile
  picture and banner. Image requests use the account's API key, are restricted
  to that account's stored media, and reject revoked keys.
- Refined the monthly TASK and RANK presentation without changing server task
  history, task calculations, rank progression, or offline behavior.
- Restyled the API Link, API Key, Save Config, Test Connection, Remove API
  Configuration, Preference dropdowns, and Save Preferences controls to match
  the NLSI pink-and-white design.
- Updated desktop, plugin, installer, and release metadata to 1.9.0 Beta.
  The stable Inno Setup AppId and existing install-location behavior remain
  unchanged.
- Kept preferences, API configuration, completed jobs, local `.nlsi` records,
  and sync state in their existing per-user locations.

## Compatibility

The desktop continues to use the existing website API forwarding routes,
authentication, monthly task and job synchronization endpoints, and local
record format. The website serves profile media only when the authenticated
API key still belongs to the account that uploaded the requested image.
No database migration is required by this release.

## Release contents

- Dashboard: `releases\1.9.0-beta\payload\nlsi_dashboard.exe`
- x64 and x86 telemetry plugins:
  `releases\1.9.0-beta\payload\plugin\win_x64\nlsi_scs_plugin.dll` and
  `releases\1.9.0-beta\payload\plugin\win_x86\nlsi_scs_plugin.dll`
- Windows installer:
  `releases\1.9.0-beta\NLSI-Exclusive-Logbook-v1.9.0-beta-Setup.exe`
- SHA-256 manifest: `releases\1.9.0-beta\manifest.sha256`

This release remains **Beta**.
