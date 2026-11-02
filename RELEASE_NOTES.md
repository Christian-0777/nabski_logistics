# NLSI Exclusive Logbook — Release Notes Index

This file is the top-level release index. The detailed release summaries are maintained in [`docs/RELEASE_HISTORY.md`](docs/RELEASE_HISTORY.md), and official release assets are published on [GitHub Releases](https://github.com/Christian-0777/nabski_logistics/releases).

## Latest verified published release

### v1.8.8 Beta

- Unified application scrollbar styling.
- Added authenticated, read-only monthly task retrieval from existing website task records.
- Added current-month TASK progress and task history to Profile, ahead of RANK.
- Preserved v1.8.7 job synchronization safeguards.
- No database migration required; missing task rows are reported as not initialized.

[Release page](https://github.com/Christian-0777/nabski_logistics/releases/tag/v1.8.8-beta)

## v1.9.4 Beta candidate

- Migrates the CMake SDK include configuration to the available SCS SDK 1.16 headers.
- Retains the existing SCS telemetry event registrations and callback signatures.
- Updates application, plugin, and installer version metadata to 1.9.4.0 / 1.9.4 Beta.
- Adds a release pipeline for x64 and x86 plugin builds, Qt runtime deployment, and the matching Qt 6.12 WebP image-format plugin.

See [v1.9.4 Beta release notes](docs/RELEASE-NOTES-v1.9.4-beta.md) and [installation notes](docs/INSTALLATION-v1.9.4-beta.md). This candidate is not identified as a published GitHub release by this documentation.

## Previous releases

| Version | Summary |
|---|---|
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

See [`docs/RELEASE_HISTORY.md`](docs/RELEASE_HISTORY.md) for the expanded summaries and limits on undocumented versions.

## Beta notice

All entries refer to Beta releases. Planned features, unverified live-game behavior, and environment-dependent API behavior must not be presented as confirmed shipped functionality. Check each release page for exact artifacts, validation results, and known limitations.
