# NLSI Exclusive Logbook — Release Notes Index

This file is the top-level release index. The detailed release summaries are maintained in `docs/RELEASE_HISTORY.md` when available, and official release assets are published on [GitHub Releases](https://github.com/Christian-0777/nabski_logistics/releases).

## Current working-tree release

### v2.3 Beta

NLSI Exclusive Logbook v2.3 Beta expands the logbook to support bus and passenger-service jobs alongside the existing truck delivery workflow.

#### Bus and passenger-service jobs

- Adds a bus-specific route and timetable workflow for passenger-service jobs.
- Initializes route stages dynamically using the captured stage count rather than relying on a fixed number of stops.
- Preserves the order of repeated SCS bus attributes so stop-specific values remain associated with their corresponding route stages.
- Retains all captured passenger-service stages through local `.nlsi` persistence.
- Displays every recorded stop in the completed-job timetable, including itineraries longer than the previously visible route stages.
- Carries available stop names, scheduled arrival/departure information, and passenger boarding/alighting values into the completed-job details.
- Uses unavailable-value presentation for fields that are not present in the captured telemetry; it does not fabricate missing stop names, times, or passenger counts.
- Supports the bus-specific Job Information, Time Table, and Driving Details presentation in the NLSI VTC Logbook when the corresponding synchronized job data is available.

#### Dashboard and job lifecycle

- Clears stale current-job dashboard fields when no active job remains.
- Preserves completed job records and XP when clearing the inactive current-job display.
- Keeps the existing truck job workflow and completed-job history.

#### Release and packaging

- Removes Completed Job PDF export and its now-exclusive report-generation helper.
- Updates application and production plugin metadata to FileVersion `2.3.0.0` / ProductVersion `2.3 Beta`.
- Packages the x64 dashboard, x64/x86 production plugins, required Qt runtime files, and managed-file manifest.

This is a working-tree Beta description. It does not by itself confirm that a v2.3 installer has been published or that a GitHub Release has been created. See the v2.3 release-specific notes and installation instructions when those files and artifacts are available.

## Latest published Beta release

### v1.9.6 Beta

- Separates active-job trailer data from idle attached-trailer telemetry.
- Improves Freight Market trailer identification using SCS Telemetry SDK 1.16 configuration and connection data.
- Retains valid job-specific trailer information through temporary telemetry gaps and prevents trailer information from leaking into subsequent jobs.
- Replaces game plugin DLLs only when NLSI ownership is established, verifies the installed DLL hash, and records deployment outcomes.
- Updates application, plugin, installer, and release metadata to v1.9.6 Beta.

[Published v1.9.6 Beta release](https://github.com/Christian-0777/nabski_logistics/releases/tag/v1.9.6-beta)

## v2.0 Beta candidate

- Expands the bus ROUTE card to show every route stop without a nested scrollbar or height cap.
- Converts planned game-time arrivals into estimated real-world clock times using the 4x SCS game-time multiplier.
- Subscribes to the SCS `game.time` channel and refreshes arrival estimates as telemetry changes.
- Removes the visible `(FALLBACK)` cargo suffix while keeping cargo source metadata.
- Upgrades recognized 1.9.7-1.9.9 Beta plugin installations to the 2.0.0.0 plugin with SHA-256 verification and rollback.
- Updates the application, plugin, and installer to 2.0 Beta.

This candidate is not identified as a published GitHub release.

## v1.9.9 Beta candidate

- Adds a bus-specific dashboard route card with dynamically initialized stages and planned timetable display.
- Preserves ordered repeated SCS bus attributes, validates stage counts, and retains route/timetable data in completed jobs.
- Updates the application and production telemetry plugin to version 1.9.9 Beta.
- Builds the x64 application and production plugin without generating an installer.

This candidate is not identified as a published GitHub release.

## v1.9.6 Beta candidate

- Shows trailer details only for the exact raw SDK markets `cargo_market` and `external_market`.
- Preserves raw market values in completed `.nlsi` job records and safely hides trailer details for older records without market data.
- Replaces game plugin DLLs only when NLSI ownership is established, verifies the installed DLL hash, and records each deployment outcome in the installer log.
- Updates application, plugin, installer, and release metadata to 1.9.6 Beta.

## v1.9.5 Beta candidate

- Adds market-aware job capture for Freight Market, Cargo Market, External Contracts, External Market, and Quick Jobs.
- Preserves the original SDK market value in `job.market.raw`.
- Adds the market to current and completed job views and generated PDF reports.
- Keeps old `.nlsi` records readable without market fields.

## v1.9.4 Beta

- Migrates the CMake SDK include configuration to the available SCS SDK 1.16 headers.
- Retains the existing SCS telemetry event registrations and callback signatures.
- Updates application, plugin, and installer version metadata to 1.9.4.0 / 1.9.4 Beta.
- Adds a release pipeline for x64 and x86 plugin builds, Qt runtime deployment, and the matching Qt 6.12 WebP image-format plugin.

[Published v1.9.4 Beta release](https://github.com/Christian-0777/nabski_logistics/releases/tag/v1.9.4-beta)

## Previous releases

| Version | Summary |
|---|---|
| 1.9.3 Beta | Corrected Windows process detection using Tool Help snapshots; exact case-insensitive ETS2/ATS executable matching; clearer enumeration errors; shared detection for startup/status/close checks; automated process-detection and handle-cleanup tests. |
| 1.9.2 Beta | Captured trailer details and plate from pre-job telemetry; prevented active-job trailer changes from overwriting saved values; corrected profile image cropping and circular clipping; added game-running startup and shutdown restrictions. |
| 1.9.1 Beta | Qt 6.12.0 x64 WebP image-format support; SCS SDK started/paused events for ACTIVE/PAUSED transitions; refined Current Job visibility while preserving active `.nlsi` records and existing API/storage/sync contracts. |
| 1.9.0 Beta | Profile account-details cleanup; authenticated profile picture/banner loading; monthly TASK and RANK presentation; API and preference control restyling; preserved per-user settings, credentials, jobs, and sync state. |
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

See the version-specific documentation and the [official Releases page](https://github.com/Christian-0777/nabski_logistics/releases) for publication status, assets, validation results, and known limitations. A candidate or working-tree entry is not necessarily a published release.

## Beta notice

All entries refer to Beta releases. NLSI Exclusive Logbook remains under active development and is not represented as stable or production-ready. Planned features, unverified live-game behavior, and environment-dependent API behavior must not be presented as confirmed shipped functionality. Check each release page for exact artifacts, validation results, and known limitations.
