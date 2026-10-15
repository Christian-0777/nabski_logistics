# NLSI Exclusive Logbook — Dashboard and Installer

**Version:** 1.7.1 Beta  
**Application:** Native Windows C++ / Qt Widgets  
**Telemetry SDK:** SCS Telemetry SDK 1.15

## Dashboard layout

- White background (`#FFFFFF`), pale-pink sidebar/panels (`#FFF7FA`), pink
  accent (`#F896B9`), light-pink borders (`#FFD1E6`), dark readable text.
- Compact native layout with modest rounded corners, no heavy shadows, and a
  minimum window size of 900 x 600.
- Keep the sidebar and header fixed while the main dashboard scrolls
  vertically. Use Qt layouts, not absolute widget positions.
- Display `DASHBOARD`, `LOCAL MODE`, and local date/time as UI metadata only.
  Never display a fabricated ping, server state, or unsupported timezone.
- Show game-process detection separately from the telemetry connection state.

The dashboard panels appear in this exact order:

1. **CURRENT JOB** — application-generated Job ID / telemetry-confirmed
   Status; Cargo / Income; From / To; Planned
   Distance / Remaining Distance; Progress bar / Arrival or ETA.
2. **DRIVING** — Fuel / Capacity; RPM / Gear; Throttle; Brake; Cruise Control;
   Retarder.
3. **GAME CONFIG** — Game / Version; Vehicle / Plate; Trailers; Trailer Plate.
4. **SUMMARY** — latest actual SDK gameplay event or relevant status notes;
   otherwise show `No events available.`

The **COMPLETED JOBS** navigation page lists only jobs finalized by SCS
`job.delivered` / `car_job.delivered` or `job.cancelled` /
`car_job.cancelled` gameplay events. It loads atomic `.nlsi` records at
startup, sorts them by finalization timestamp, and refreshes on each confirmed
terminal event. Active job records are retained separately for restart
recovery and are never included in this list.

Unavailable values display an em dash (`—`). Use only raw fields provided by
verified SDK channels, SDK configuration callbacks, or SDK events. Job IDs
are application-owned, persisted sequence values; job status comes from SCS
configuration and gameplay events. Navigation
distance and ETA are displayed as raw SDK values with documented units; do not
derive progress, remaining distance, or an arrival clock. Fuel capacity comes
only from truck configuration. `game_version` is the telemetry API version,
not the retail game version.

## Process detection and connection states

Detect the Windows process names `eurotrucks2.exe` and `amtrucks.exe`
periodically without blocking the Qt GUI thread. Report **NOT RUNNING**, **ETS2
RUNNING**, **ATS RUNNING**, **BOTH RUNNING**, or **DETECTION ERROR**. Do not
silently choose one game when both are running.

Telemetry connection state is independent: **Disconnected**, **Connecting**,
**Connected**, or **Error**. Only use fresh IPC snapshots whose SDK game ID
matches the detected process. Stale frames and malformed messages must not
appear as current data.

## Read-only behavior

- Keep the game-loaded SCS plugin and Qt dashboard as separate native
  components; exchange telemetry over the local named pipe
  `\\.\pipe\nlsi_scs_telemetry`.
- Job records use atomic JSON-formatted `.nlsi` files and a separate monotonic
  sequence file under the user's application-data directory.
- No remote server, telemetry upload, game-control command, or game-state
  modification.
- Do not terminate or modify either game process.
- Do not disable, rename, remove, or overwrite third-party plugins.

## Installer

**Product:** NLSI Exclusive Logbook  
**Version:** 1.7.1 Beta  
**Default application directory:** `C:\Program Files\Nabski Logistics Solutions Inc.`

Support fresh and update installations. Package the Qt and MSVC runtime
dependencies with the application. Preserve unrelated files and settings on
update, and close the NLSI dashboard safely before replacing its files. Job
records and the sequence are kept outside the install directory and are
preserved on update and uninstall.

Offer separate optional plugin installation tasks only when the corresponding
default Steam game folder exists:

| Game | x64 DLL destination | x86 DLL destination |
|---|---|---|
| ETS2 | `C:\Program Files (x86)\Steam\steamapps\common\Euro Truck Simulator 2\bin\win_x64\plugins` | `C:\Program Files (x86)\Steam\steamapps\common\Euro Truck Simulator 2\bin\win_x86\plugins` |
| ATS | `C:\Program Files (x86)\Steam\steamapps\common\American Truck Simulator\bin\win_x64\plugins` | `C:\Program Files (x86)\Steam\steamapps\common\American Truck Simulator\bin\win_x86\plugins` |

The installer blocks if ETS2 or ATS is running and never terminates either
game. It detects the default Steam location and registered Steam library
folders, then offers only plugin architectures for which a matching game
executable and architecture directory exist. It may replace only NLSI-owned
plugin files whose ownership marker is valid and whose recorded hash has not
changed; never overwrite third-party or user-modified files. It backs up a
marked NLSI plugin before updating it. Do not terminate ETS2 or ATS. Validate
both plugin architectures and required SCS exports before packaging. Do not
report fresh/update installation tests as passed unless performed.
