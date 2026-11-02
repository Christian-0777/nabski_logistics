# NLSI Exclusive Logbook v1.9.2 Beta Release Notes

## Changes

- Capture trailer details and plate from the available pre-job telemetry state.
  Active-job trailer updates no longer replace the captured values in the
  dashboard, local `.nlsi` record, completed-job details, or PDF report. If no
  trailer was present before the job, the record displays `—`.
- Correct profile image rendering with centered aspect-preserving cropping,
  smooth scaling, and circular clipping. The existing authenticated request,
  fallback placeholder, and JPEG/PNG/WebP support remain in place.
- Block application startup when `eurotrucks2.exe` or `amtrucks.exe` is
  detected. Close attempts recheck both processes and remain blocked while a
  game is running; process-detection failures also block the operation.
- Update desktop, plugin, installer, and release metadata to 1.9.2 Beta.

## Compatibility

No `.nlsi` record, profile, or settings migration is required. The installer
retains the existing AppId and continues to leave per-user preferences,
credentials, and job history outside its managed payload.

## Release contents

- Dashboard: `releases\1.9.2-beta\payload\nlsi_dashboard.exe`
- x64 and x86 telemetry plugins:
  `releases\1.9.2-beta\payload\plugin\win_x64\nlsi_scs_plugin.dll` and
  `releases\1.9.2-beta\payload\plugin\win_x86\nlsi_scs_plugin.dll`
- Qt runtime and image-format plugins, including `imageformats\qwebp.dll`
- Windows installer:
  `releases\1.9.2-beta\NLSI-Exclusive-Logbook-v1.9.2-beta-Setup.exe`
- SHA-256 manifest: `releases\1.9.2-beta\manifest.sha256`

This release remains **Beta**.
