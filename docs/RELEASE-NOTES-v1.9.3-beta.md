# NLSI Exclusive Logbook v1.9.3 Beta Release Notes

## Changes

- Fix Windows process detection by using the Windows Tool Help process
  snapshot API under the Windows platform macro. ETS2 and ATS are matched by
  exact executable filename, case-insensitively.
- Keep startup and shutdown blocked when either or both supported games are
  running. Process-enumeration failures remain fail-closed and now report a
  specific Windows error code when available instead of claiming a game is
  running.
- Reuse the same process detector for startup, periodic status updates, and
  every close attempt. Snapshot handles are closed on success and failure.
- Add automated tests for independent ETS2/ATS detection, exact matching,
  enumeration failures, close-state decisions, and repeated snapshot cleanup.

## Compatibility

No `.nlsi` record, profile, settings, or configuration migration is required.
The installer retains the existing AppId and continues to leave per-user
preferences, credentials, and job history outside its managed payload.

## Release status

This release remains **Beta**.
