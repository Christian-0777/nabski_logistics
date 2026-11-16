# NLSI Exclusive Logbook

**Windows telemetry dashboard and delivery logbook for Euro Truck Simulator 2 and American Truck Simulator.**

[![Current working-tree release](https://img.shields.io/badge/release-v2.3%20Beta-e05a98)](RELEASE_NOTES.md)
[![Status](https://img.shields.io/badge/status-beta-orange)](https://github.com/Christian-0777/nabski_logistics/releases)

> **Project status: Beta / active development.** Current working-tree version: **NLSI Exclusive Logbook 2.3 Beta**. The v2.3 working-tree build is not represented as a published GitHub release.

## Download

- **[Current 2.3 Beta release notes](RELEASE_NOTES.md#v23-beta)**
- [Latest published v1.9.6 Beta release](https://github.com/Christian-0777/nabski_logistics/releases/tag/v1.9.6-beta)
- [View all published releases](https://github.com/Christian-0777/nabski_logistics/releases)
- [Detailed release notes index](RELEASE_NOTES.md)

Download installers only from the project's official GitHub Releases page. A working-tree build or release candidate is not an official downloadable release until it is published on GitHub Releases.

## Supported games

| Game | Executable | Telemetry integration |
|---|---|---|
| Euro Truck Simulator 2 (ETS2) | `eurotrucks2.exe` | SCS Telemetry SDK integration |
| American Truck Simulator (ATS) | `amtrucks.exe` | SCS Telemetry SDK integration |

Telemetry availability can depend on the installed game version, SDK compatibility, plugin deployment, and the data exposed by the game. Support for other games is not claimed.

## Main features

- Dashboard for available vehicle, driving, navigation, and job telemetry.
- Current-job information and completed-job history.
- Truck job telemetry and delivery records.
- **Bus / passenger-service job support**, including route stages, ordered stops, planned timetable information, and passenger boarding/alighting values where provided by telemetry.
- Dynamic bus route presentation that supports the recorded stage count instead of assuming a fixed number of stops.
- Local delivery/session records in the application's `.nlsi` format.
- Local communication between the game telemetry plugin and desktop application.
- Optional authenticated NLSI VTC profile and job synchronization when configured.
- Profile and VTC information where supported by the configured API.
- Local records remain available when the remote API is unavailable.

Feature availability changes between Beta versions. Refer to the release summaries and detailed release notes. Bus timetable fields depend on the attributes exposed and captured from the supported game telemetry.

## Bus and passenger-service jobs

NLSI 2.3 Beta expands job handling beyond conventional cargo deliveries to include bus/passenger-service jobs.

The bus workflow is designed to:

- Identify and present bus jobs separately from ordinary truck cargo jobs.
- Initialize and display route stages dynamically from the captured stage count.
- Preserve the original order of repeated SCS bus attributes so that stop-specific values remain associated with the correct stage.
- Retain the available passenger-service stops and timetable data in completed `.nlsi` job records.
- Display the recorded stops in the completed-job timetable rather than truncating the itinerary to a fixed number of rows.
- Show scheduled arrival/departure and passenger boarding/alighting values when those values are present in the captured record.
- Keep missing telemetry values distinguishable from actual zero values; unavailable values may be displayed as an em dash.

The timetable is based on recorded telemetry. It does not invent stop names, times, or passenger counts when the source record does not contain them.

## Installation guide

### Fresh installation

1. Open the official [GitHub Releases page](https://github.com/Christian-0777/nabski_logistics/releases).
2. Select a published release. The v2.3 Beta working-tree build is not a published download unless a corresponding release page and installer asset are available.
3. In the release's **Assets** section, download the versioned Windows installer (`.exe`).
4. Close NLSI Exclusive Logbook and exit ETS2/ATS before installing. This helps avoid locked game-plugin files.
5. Run the installer and follow its instructions. Review the installation directory and any game-plugin deployment locations shown by the installer.
6. Complete installation and launch NLSI Exclusive Logbook.
7. Start one supported game and verify that telemetry is received. Test ETS2 and ATS separately if both are installed.
8. Configure the NLSI VTC connection only if you intend to use account/profile synchronization. Keep API keys and credentials private.

### Updating an existing installation

1. Back up important `.nlsi` records, application settings, configuration, and exported reports.
2. Close NLSI Exclusive Logbook and both supported games.
3. Download the desired version's installer from the official [GitHub Releases](https://github.com/Christian-0777/nabski_logistics/releases) page.
4. Read that version's release notes for upgrade instructions and known issues.
5. Run the installer. Perform an in-place upgrade only when supported by that release's installer and notes.
6. Launch the application and verify the displayed version, telemetry plugin, local job history, and configured account connection.

Do not manually delete game plugin files or local application data unless the relevant release instructions explicitly require it. Preserve user data and unrelated third-party plugins.

### Troubleshooting

- **No telemetry:** Confirm that the game is supported, the correct application release is installed, and its telemetry plugin was deployed as instructed.
- **Bus route or timetable is incomplete:** Check whether the relevant bus stages and attributes were captured in the job record. The application cannot display source values that were not supplied by telemetry.
- **Installer cannot update a plugin:** Exit the game and application, then retry the installer.
- **Account synchronization unavailable:** Check the configured API connection and account authentication. Local telemetry and local job history are separate from remote API availability.
- **Requesting support:** Include the application version and a concise description. Remove API keys, passwords, tokens, personal information, and private local-record data from logs or screenshots.

## Release summaries

The following summaries describe changes recorded in the project's release documentation. All versions listed are **Beta**. A working-tree version or candidate is not necessarily a published GitHub release. See [RELEASE_NOTES.md](RELEASE_NOTES.md), the individual versioned notes in the repository, and the [official Releases page](https://github.com/Christian-0777/nabski_logistics/releases) for publication status and downloadable assets.

| Version | Summary of documented changes |
|---|---|
| **2.3 Beta** | Adds bus/passenger-service job support; dynamically handles route stages; preserves ordered repeated bus attributes and all captured passenger-service stops in `.nlsi` records; displays the completed-job timetable; clears stale current-job dashboard fields; removes Completed Job PDF export; updates application/plugin metadata. |
| **2.0 Beta** | Expands the bus route card to show all stops; estimates real-world arrival clock times from game time; subscribes to the SCS `game.time` channel; updates recognized plugin installations with hash verification and rollback. |
| **1.9.9 Beta** | Adds a bus-specific dashboard route card with dynamically initialized stages and planned timetable display; preserves ordered repeated SCS bus attributes and route/timetable data in completed jobs. |
| **1.9.6 Beta** | Separates active-job trailer data from idle attached-trailer telemetry; improves Freight Market trailer identification, retains valid job data through temporary telemetry gaps, and clears trailer fields between jobs. |
| **1.9.5 Beta** | Captures Freight Market trailer details from SCS SDK 1.16 configuration and connection telemetry; retains active-job trailer values and clears them on confirmed detachment. |
| **1.9.4 Beta** | Migrates the Windows build and telemetry plugin to SCS SDK 1.16; retains the existing telemetry events and callback API; updates version metadata and release packaging. |
| **1.9.3 Beta** | Corrected Windows process detection using Tool Help snapshots; exact case-insensitive ETS2/ATS executable matching; clearer enumeration errors; shared detection for startup/status/close checks; automated process-detection and handle-cleanup tests. |
| **1.9.2 Beta** | Captured trailer details and plate from pre-job telemetry; prevented active-job trailer changes from overwriting saved values; corrected profile image cropping and circular clipping; added game-running startup and shutdown restrictions. |
| **1.9.1 Beta** | Deployed Qt 6.12.0 x64 WebP image-format support; used SCS SDK `started` and `paused` events for ACTIVE/PAUSED transitions; refined Current Job visibility while preserving active `.nlsi` records and existing API/storage/sync contracts. |
| **1.9.0 Beta** | Removed duplicate Profile account-details card; added authenticated profile picture/banner loading; refined monthly TASK and RANK presentation; restyled API and preference controls; preserved per-user settings, credentials, jobs, and sync state. |
| **1.8.8 Beta** | Unified scrollbar styling; authenticated read-only monthly task retrieval; monthly TASK progress and history on Profile; retained v1.8.7 job-sync safeguards. |
| **1.8.7 Beta** | Expanded eligible-role completed-job synchronization; account ownership checks; retryable failed uploads; strict server acknowledgement; Profile TASK placeholder. |
| **1.8.6 Beta** | Safer origin/destination country formatting; release metadata and staging checks. |
| **1.8.5 Beta** | Authentication and profile refresh corrections; API diagnostics; job-sync route forwarding; ngrok JSON response handling. |
| **1.8.4 Beta** | Clearer online/offline/local status; profile identity separation; local history preservation; strict sync acknowledgement. |
| **1.8.2 Beta** | Account validation; local-data preservation; driver invitation/password workflow; release metadata. |
| **1.8.1 Beta** | Encrypted API credential removal; account revalidation before synchronization. |
| **1.8.0 Beta** | Profile page; account connectivity; local mode; optional API configuration; local data preservation. |
| **1.7.5 Beta** | Summary event history; currency display; numeric formatting; installer improvements. |
| **1.7.4 Beta** | Preferences; encrypted DPAPI profile storage; centralized display conversions; PDF reporting. |
| **1.7.3 Beta** | Telemetry provider stability; active-job field resilience; brake/retarder display; managed plugin update behavior. |
| **1.7.2 Beta** | Dashboard sidebar and telemetry presentation refinements. |

The available release documentation does not establish a reliable feature-by-feature summary for every earlier version or patch. Missing entries are intentionally not guessed.

## License and third-party credits

### Project ownership and license

NLSI Exclusive Logbook is an NLSI project. The repository documentation does not establish a specific open-source license for the application's own code, branding, or assets. Unless the project owner publishes a license granting those rights, do not assume that the application may be copied, modified, or redistributed.

### Third-party software and plugin ownership

| Software / component | Owner or maintainer | Role / attribution |
|---|---|---|
| Euro Truck Simulator 2 | **SCS Software** | Supported game; game telemetry source. |
| American Truck Simulator | **SCS Software** | Supported game; game telemetry source. |
| SCS Telemetry SDK | **SCS Software** | Official telemetry interface used by the game's integration. The SDK remains subject to SCS Software's terms. |
| NLSI SCS telemetry plugin / integration | **NLSI project** | Project-maintained integration code that connects supported game telemetry to the logbook. It is not an SCS-authored plugin. |
| Qt framework / Qt Widgets | **The Qt Company and Qt Project** | Desktop application framework and user-interface components. |
| Qt Image Formats WebP plugin (`qwebp`, when included) | **Qt Project / The Qt Company** | WebP image-format support. The applicable version and license must match the actual distributed Qt module. |

The table identifies known owners and roles. If a particular release bundles additional third-party DLLs, plugins, libraries, or tools, its release package should include or reference the exact upstream project, copyright holder, version, and license notice. Do not attribute an unverified binary to NLSI or another vendor by assumption.

### Third-party license notices

- **SCS Software / SCS Telemetry SDK:** governed by SCS Software's applicable SDK terms and conditions. Do not redistribute SDK materials unless those terms permit it.
- **Qt:** governed by the license applicable to the exact Qt version, modules, and distribution model. See [Qt licensing](https://www.qt.io/licensing).
- **Other bundled components:** governed by their respective upstream licenses and notices. Review the notices shipped with the exact release before redistribution.

Euro Truck Simulator 2, American Truck Simulator, SCS Software names, logos, and game assets belong to their respective owners. They are referenced to identify compatibility and ownership; this project does not claim ownership of them or imply SCS Software endorsement.

## Privacy and security

- Game telemetry is communicated locally between the game plugin and desktop application.
- Local delivery history can be retained without a working remote account connection.
- Optional NLSI VTC profile and job synchronization requires the configured API endpoint and valid account authentication.
- Do not publish API keys, passwords, access tokens, private profile data, `.nlsi` records, or unredacted logs.
- Do not expose a database server directly to the public internet.
- Report suspected security issues privately to the project maintainer rather than publishing credentials or exploit details in public issues.

---

**NLSI Exclusive Logbook — Beta software.** Features, compatibility, and behavior may change between releases.
