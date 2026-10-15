# NLSI Exclusive Logbook 1.7.3 Beta

## What's new

- Keep a single live telemetry provider selected until it disconnects or
  becomes stale, avoiding alternating same-game snapshots.
- Preserve valid active-job route and status values across transient missing
  fields while still applying valid job starts, route changes, and completions.
- Display BRAKE and RETARDER `A` markers only while SDK cruise control is
  active, without changing their measured brake percentage or retarder level.
- Automatically replace unchanged NLSI-managed x64/x86 game plugins during
  fresh installs and in-place upgrades, with explicit failure reporting for
  locked DLLs.
- Keep FUEL COST unavailable because the integrated SDK data has no reliable
  refueling price. REFUELED and TOTAL FUEL CONSUMED remain separate metrics.
- Update application and installer metadata to 1.7.3 Beta.

Fuel cost requires a reliable per-refuel monetary amount or a price-per-liter
source with currency and quantity; no value is inferred from liters alone.
Installer upgrade and fresh-install scenarios require manual verification
against a test installation. No live ETS2/ATS validation is implied by the
release build.
