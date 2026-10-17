# NLSI Exclusive Logbook v1.8.8 Beta Release Notes

## Changes

- Unified vertical and horizontal scrollbar styling across the dashboard,
  completed-job list, Profile, and dialogs using the existing NLSI pink/white
  colors. Scrollbar tracks, thumbs, dimensions, borders, radii, hover, pressed,
  and disabled states are styled without disabling scrolling.
- Added `GET /monthly-task.php` to the shared API and the website forwarding
  route. It uses bearer-key authentication and obtains the driver identity
  from the validated account; clients cannot select another driver.
- The API reads only existing `monthly_tasks` records and returns the
  current Manila-calendar month, current row (or `null` when not initialized),
  and task history. Each record carries its stored target, completed
  distance, status, completion/finalization timestamps, and progress.
- The Profile TASK card displays real monthly progress, target, completion
  amount, month, status, completion state, and prior task history above RANK.
  Missing task rows, malformed responses, and API/offline failures are
  distinguished; offline progress is not invented or cached.
- The existing DRIVER, MANAGER, CEO, and ADMIN job synchronization behavior
  from v1.8.7 Beta remains in place. Account ownership, retryable jobs,
  duplicate prevention, and committed server acknowledgements are preserved.
- No database migration or data change is required.

## Validation

- CTest Release: 5/5 passed.
- Website PHP tests: 8/8 passed.
- Shared API PHP tests: 4/4 passed.
- The fresh release pipeline performs the x64 application build and CTest,
  builds the static-runtime SCS plugins for x64 and x86, verifies architecture,
  exports, and version resources, stages a new version-only payload, deploys
  Qt/MSVC runtime dependencies, and compiles the Inno Setup installer.
- The release pipeline also checks the complete staged payload and records
  SHA-256 hashes for the installer, executable, both plugin DLLs, and release
  documents in `manifest.sha256`.

## Artifacts

From the desktop project root:

- Payload: `releases\1.8.8-beta\payload\`
- Application: `releases\1.8.8-beta\payload\nlsi_dashboard.exe` (Windows x64)
- ETS2/ATS plugins:
  `releases\1.8.8-beta\payload\plugin\win_x64\nlsi_scs_plugin.dll` (x64) and
  `releases\1.8.8-beta\payload\plugin\win_x86\nlsi_scs_plugin.dll` (x86)
- Installer:
  `releases\1.8.8-beta\NLSI-Exclusive-Logbook-v1.8.8-beta-Setup.exe`
- Hash manifest: `releases\1.8.8-beta\manifest.sha256`

## Known limitation

The API does not create missing current-month task rows. Existing website task
processing must initialize the row. Until then, Profile says that the month is
not initialized and continues to show any stored history. The desktop does
not cache task progress while offline.
