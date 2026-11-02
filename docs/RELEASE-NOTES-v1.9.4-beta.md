# NLSI Exclusive Logbook v1.9.4 Beta Release Notes

## Changes

- Migrate the CMake SDK include configuration from the missing SDK 1.15
  directory to the supplied SCS Telemetry SDK 1.16 headers.
- Keep the existing SCS initialization/shutdown exports, event registrations,
  event handling, and callback signatures.
- Update application, plugin, and installer metadata to 1.9.4.0 / 1.9.4 Beta.
- Update the release pipeline to stage x64 and x86 telemetry plugins, the Qt
  MSVC runtime, and the matching Qt 6.12 WebP image-format plugin.

## Compatibility

No `.nlsi` record, profile, settings, or configuration migration is required.
The installer retains the existing AppId and keeps per-user records,
preferences, and credentials outside its managed payload.

## Verification limits

- The MSVC x64 Release build completed and all seven automated test targets
  passed.
- The x64 and x86 plugin PE architectures and required exports were verified.
- The packaged Qt WebP decoder test and isolated dashboard startup smoke test
  passed.
- The installer compiled. Fresh install, upgrade, and uninstall were not tested
  because this session is not elevated; no v1.9.3 installer was available for
  an upgrade test.
- No live ETS2/ATS game session or remote API service was tested.

This release remains **Beta**.
