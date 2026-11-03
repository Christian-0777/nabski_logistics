
# NLSI Exclusive Logbook — Release Notes Index

This file is the top-level release index. The detailed release summaries are maintained in [`docs/RELEASE_HISTORY.md`](docs/RELEASE_HISTORY.md), and official release assets are published on [GitHub Releases](https://github.com/Christian-0777/nabski_logistics/releases).

## Latest release

### v1.9.6 Beta

- Restructures trailer data handling so active-job trailer details and plate are maintained independently from idle attached-trailer telemetry.
- Improves Freight Market trailer identification using SCS Telemetry SDK 1.16 configuration attributes and documented connection channels.
- Shows the actual trailer and trailer plate associated with the active job, without relying exclusively on the currently attached-trailer state.
- Displays an em dash (`—`) in trailer and trailer plate fields when there is no active job, even if a trailer is attached while idle.
- Retains valid job-specific trailer values during temporary telemetry gaps and prevents trailer information from leaking into subsequent jobs.
- Preserves the existing `.nlsi` record format.
- Automated job-history and dashboard tests cover completed records and details. PDF generation passed, but trailer-text extraction from exported PDFs was not separately asserted.
- The release build report records successful x64 application and x64/x86 plugin builds, packaged-runtime checks, and installer compilation.
- The test report records 7/7 automated tests passing.
- ETS2/ATS in-game testing was not performed.

See [v1.9.6 Beta release notes](docs/RELEASE-NOTES-v1.9.6-beta.md) and [installation notes](docs/INSTALLATION-v1.9.6-beta.md).

## Previous release candidates

### v1.9.5 Beta

- Captures Freight Market trailer identity, details, and plate from SCS SDK 1.16 configuration and connection telemetry.
- Retains valid trailer values across temporary missing updates and clears active-job trailer data on an explicit detached state.
- Preserves truck-job telemetry and the existing `.nlsi` record format.

See [v1.9.5 Beta release notes](docs/RELEASE-NOTES-v1.9.5-beta.md) and [installation notes](docs/INSTALLATION-v1.9.5-beta.md).

### v1.9.4 Beta

- Migrates the CMake SDK include configuration to the available SCS SDK 1.16 headers.
- Retains the existing SCS telemetry event registrations and callback signatures.
- Updates application, plugin, and installer version metadata to 1.9.4.0 / 1.9.4 Beta.
- Adds a release pipeline for x64 and x86 plugin builds, Qt runtime deployment, and the matching Qt 6.12 WebP image-format plugin.

See [v1.9.4 Beta release notes](docs/RELEASE-NOTES-v1.9.4-beta.md) and [installation notes](docs/INSTALLATION-v1.9.4-beta.md).

## Previous releases

| Version | Summary |
|---|---|
| 1.8.8 Beta | Unified application scrollbar styling, authenticated read-only monthly task retrieval, current-month TASK progress and task history in Profile, and preserved v1.8.7 synchronization safeguards. |
| 1.8.7 Beta | Broader eligible-role job sync, account ownership checks, retryable failures, strict server acknowledgement, Profile TASK placeholder. |
| 1.8.6 Beta | Safer origin/destination country formatting and release metadata/staging checks. |
| 1.8.5 Beta | Authentication/profile refresh corrections, API diagnostics, job-sync route forwarding, ngrok JSON response handling. |
| 1.8.4 Beta | Clearer online/offline/local status, profile identity separation, local history preservation, strict sync acknowledgement. |
| 1.8.2 Beta | Account validation, local-data preservation, driver invitation/password workflow and release metadata. |
| 1.8.1 Beta | Encrypted API credential removal and account revalidation before sync. |
| 1.8.0 Beta | Profile page, account connectivity, local mode, optional API configuration, and data preservation. |
| 1.7.5 Beta | Summary event history, currency display, numeric formatting, installer improvements. |
| 1.7.4 Beta | Preferences, encrypted DPAPI profile storage, centralized display conversions, PDF reporting. |
| 1.7.3 Beta | Telemetry provider stability, active-job field resilience, brake/retarder display, managed plugin update behavior. |
| 1.7.2 Beta | Dashboard sidebar and telemetry presentation refinements. |

See [`docs/RELEASE_HISTORY.md`](docs/RELEASE_HISTORY.md) for expanded summaries and limits on undocumented versions.

## Beta notice

All entries refer to Beta releases. Planned features, unverified live-game behavior, and environment-dependent API behavior must not be presented as confirmed shipped functionality. Check each release's publication status, release page, artifacts, validation results, and known limitations before treating it as an officially published release.
