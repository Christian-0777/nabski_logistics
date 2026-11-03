# NLSI EXCLUSIVE LOGBOOK v1.9.6 BETA

## Main changes

- Restructures trailer data handling across SCS configuration callbacks,
  connection telemetry, IPC snapshots, the dashboard, and job history.
- Separates active-job trailer information from idle attached-trailer
  telemetry. The active job's trailer and trailer plate are displayed from
  job-specific data, not solely from the currently attached trailer.
- Improves Freight Market trailer identification using SCS Telemetry SDK 1.16
  trailer configuration attributes and documented connection channels.
- Displays an em dash (`—`) in the trailer and trailer plate fields when there
  is no active job.
- Preserves valid job-specific trailer values during temporary telemetry gaps,
  while preventing trailer information from leaking into a subsequent job.
- Keeps the existing `.nlsi` record format and truck-job telemetry behavior.

Completed `.nlsi` records and completed-job details retain the trailer and
plate recorded for that job, as covered by automated job-history and dashboard
tests. The completed-job PDF uses the same saved trailer fields. PDF generation
was tested, but the release test report does not record a separate assertion
that extracts and compares the trailer text in the generated PDF.

## Technical changes

- Maps SCS SDK trailer configuration data to trailer identity, descriptive
  details, and license plate fields.
- Uses the job-specific trailer snapshot for active-job display and saved job
  data; an explicit detached state clears trailer details for that job.
- Keeps idle telemetry from replacing active-job trailer information and
  clears displayed trailer fields when no job is active.
- Carries trailer values through existing IPC and `.nlsi` serialization
  without requiring a record-format migration.

## Compatibility

- The Windows dashboard is x64. Release payloads include x64 and x86 SCS
  Telemetry plugin DLLs for matching installed game architectures.
- Uses SCS Telemetry SDK 1.16. No `.nlsi` record migration is required.
- The installer retains the existing product AppId and preserves per-user
  preferences, credentials, completed-job history, synchronization state, and
  `.nlsi` records outside its application payload.

## Testing performed

The v1.9.6 Beta build and test reports in `releases\1.9.6-beta` record:

- Fresh Windows x64 application and x64/x86 plugin builds, with architecture,
  SCS exports, versions, and staged hashes checked.
- 7/7 configured automated tests passed; the focused telemetry, job-history,
  and dashboard trailer regression set passed 3/3.
- Packaged JPEG/PNG/WebP decoder and offscreen dashboard startup checks passed.
- The production installer compiled successfully.
- An isolated installer upgrade test verified replacement of marked,
  hash-verified NLSI plugin DLLs and preservation of simulated preferences,
  credentials, job history, synchronization state, and a `.nlsi` record.

ETS2 and ATS were not launched. No in-game telemetry behavior is claimed.
The upgrade test used an isolated temporary installation; the production
installer was not run against the user's actual game directories.

## Known limitations

- Live ETS2/ATS telemetry and Freight Market behavior have not been verified
  in-game.
- No reboot-time replacement of a locked DLL was simulated.
- Trailer data availability depends on the game and SDK telemetry exposed at
  runtime. Missing telemetry can leave a field unavailable until valid
  job-specific data is received.
- The generated PDF was checked for successful PDF output; trailer-text
  extraction from the PDF was not separately tested.

## Installation and upgrade

See [Installation — v1.9.6 Beta](INSTALLATION-v1.9.6-beta.md) for the installer
location, supported architectures, plugin deployment, upgrade preservation,
and known installation limits.

This is a **Beta** release candidate. Its local artifacts are not identified
as published on GitHub by this documentation.
