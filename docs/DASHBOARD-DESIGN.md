# Dashboard design notes

The dashboard follows the supplied reference mockup and keeps the palette intentionally close to the provided visual specification.

## Layout

- Fixed pale-pink sidebar wide enough for the logo and single-line
  `LOGISTICS SOLUTIONS INC.` branding, alongside a white main panel
- White telemetry cards with consistent spacing, pink borders, and readable
  typography
- Centralized dashboard font sizes in `src/DashboardTypography.h`: NABSKI
  16px, company subtitle 13px, header titles 15px, field labels and values
  14px, and card titles 13px
- A fixed header showing `DASHBOARD`, `LOCAL MODE`, the local date/time as UI
  metadata, game-process detection, and telemetry connection as separate states
- Four scrollable telemetry cards in order: Current Job, Driving, Game Config,
  and Summary
- Progress is displayed as the existing calculated percentage text, without a
  graphical progress bar. Arrival and fuel values retain their existing
  formatting and telemetry sources.
- Game Config identifies the detected game in its heading and displays the
  existing API version above two aligned rows for vehicle, vehicle plate,
  trailer, and trailer plate.
- A minimum window size of 900 x 600

## Read-only behavior

- No telemetry persistence to disk
- No database access
- No remote network access
- No game-control or gameplay-state mutation
- Unsupported values display an em dash (`—`)
- The data shown is raw values only when they are directly exposed by the official SCS SDK headers

## State model

The dashboard distinguishes the following states:

1. Disconnected — no fresh frame for the detected game.
2. Connecting — the detected game's plugin has connected but has not delivered a
   valid frame yet.
3. Connected — a valid, fresh frame for the detected game has arrived.
4. Error — IPC failed, a malformed frame arrived, process detection failed, or
   both games are running and the source cannot be selected safely.

Game-process detection independently reports not running, ETS2, ATS, both, or
detection error. A telemetry connection is never inferred from the process
being present.

The game name and telemetry API version come from SDK initialization. The API
version is not presented as the game's retail patch version. No ping or server
status is shown. Summary remains in a neutral empty state until actual events
are wired.

## IPC framing

The plugin sends escaped UTF-8 line records to the dashboard over the local
named pipe `\\.\pipe\nlsi_scs_telemetry`. One complete snapshot is terminated
by an empty line; periodic frames act as a liveness heartbeat. The plugin queues
snapshots to a worker thread so telemetry callbacks do not perform IPC writes.
Each dashboard socket has an independent receive buffer and stale samples are
discarded.

## Source fidelity

This implementation uses the shared local IPC schema and intentionally exposes only fields that are present in the SDK headers. It does not invent telemetry fields to fill the empty spaces.
