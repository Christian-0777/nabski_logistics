# NLSI Exclusive Logbook v1.9.1 Beta Release Notes

## Changes

- Deploy the Qt 6.12.0 x64 WebP image-format plugin with the desktop runtime so
  authenticated profile pictures and banners stored as WebP can be decoded.
- Use the SCS SDK `started` and `paused` events to report ACTIVE/PAUSED job
  transitions and distinguish driving mode from game process and pipe state.
- Hide the Current Job value while the game is closed, telemetry is disconnected,
  the player is outside driving mode, or telemetry reports no current job. Keep
  the persisted active `.nlsi` record unchanged while the game is closed.
- Keep the existing authenticated media requests, API fields, local storage
  format, synchronization contract, and installer AppId unchanged.
- Update application, telemetry plugin, installer, and release metadata to
  1.9.1 Beta.

## Compatibility

The desktop continues to use the existing website API, account authentication,
job synchronization endpoints, and local record format. The telemetry plugin
uses the SDK's documented `SCS_TELEMETRY_EVENT_started` and
`SCS_TELEMETRY_EVENT_paused` callbacks for driving-mode transitions. ETS2 and
ATS remain supported.

## Release contents

- Dashboard: `releases\1.9.1-beta\payload\nlsi_dashboard.exe`
- x64 and x86 telemetry plugins:
  `releases\1.9.1-beta\payload\plugin\win_x64\nlsi_scs_plugin.dll` and
  `releases\1.9.1-beta\payload\plugin\win_x86\nlsi_scs_plugin.dll`
- Qt runtime and image-format plugins, including `imageformats\qwebp.dll`
- Windows installer:
  `releases\1.9.1-beta\NLSI-Exclusive-Logbook-v1.9.1-beta-Setup.exe`
- SHA-256 manifest: `releases\1.9.1-beta\manifest.sha256`

This release remains **Beta**.
