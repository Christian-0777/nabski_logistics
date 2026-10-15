# NLSI Exclusive Logbook 1.6.9 Beta

## Release contents

- Windows x64 Qt Widgets dashboard with product version `1.6.9 Beta`.
- SCS Telemetry SDK 1.15 plugins built for x64 and x86 game processes.
- Read-only local telemetry with the existing pink-and-white dashboard design.
- An x64 installer supporting fresh installation and in-place updates.

## Telemetry and dashboard

- Converts the SDK navigation advisor ETA from game-time seconds to real-time
  duration at 15:1, with an estimated local arrival clock and day rollover.
- Displays cruise-control, applied brake-pedal, and engaged retarder indicators
  from their corresponding SDK telemetry semantics.
- Formats fuel and RPM to two decimal places and highlights fuel at or below
  20% of the configured tank capacity.
- Shows unavailable fuel percentages as an em dash when the amount or capacity
  cannot produce a valid calculation.
- Preserves the v1.6.8 dashboard layout, typography, uppercase labels, branding,
  and field positions.

The SDK defines `truck.navigation.time` as an advisor ETA in seconds but does
not explicitly state whether those seconds are game-time or real-time. The
dashboard applies the requested 15:1 game-time conversion once; see
`SDK-CHANNEL-INVENTORY.md` for this documented limitation.

## Installation and updates

The installer replaces application files from the new payload, preserves
user-created files outside the installer-managed payload, and protects
unmarked or modified third-party plugins. Eligible, unchanged NLSI plugins
are updated in place for each installed ETS2/ATS x64 and x86 game directory.
The dashboard and game processes are handled before replacing locked files.
