# NLSI Exclusive Logbook 1.7.2 Beta

## What's new

- Refined the sidebar navigation with full-width active states, consistent
  button sizing, brand-colored selection, and a separated version footer.
- Kept the job status display stable on unchanged telemetry refreshes while
  continuing to update it when the reported status changes.
- Show brake input and retarder level values directly; show the cruise-control
  active marker only when the SDK cruise-control speed is nonzero.
- Display Current Job origin and destination using available company and city
  telemetry in `COMPANY - CITY` format.
- Standardized displayed field values as uppercase and bold without modifying
  persisted job data.
- Updated application, installer, and release metadata to v1.7.2 Beta.

## Compatibility and data

The dashboard sections and telemetry field positions are unchanged. Existing
job history, user data locations, and recognized NLSI-managed plugin update
protections remain in place.

## Validation

The release build validates the x64 dashboard and plugin, separately builds
the x86 plugin, runs the automated tests, stages the runtime payload, and
checks that the installer packages the application and both architecture-
matched plugin DLLs. Live ETS2/ATS gameplay validation is not implied by a
successful build or automated test run.
