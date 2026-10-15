# NLSI Exclusive Logbook 1.7.0 Beta

## What's new

- Application-generated job IDs use the `NLSI-0001` sequence and continue
  beyond four digits. The sequence and active job survive application restarts.
- Confirmed SCS gameplay delivery and cancellation events flow through the
  plugin and local IPC as `DELIVERED` and `CANCELLED`. Distance reaching zero
  is not used to infer a completed job.
- Completed and cancelled jobs are stored individually as atomic `.nlsi`
  JSON records in the user's application-data directory. Malformed records
  are reported and skipped without preventing other records from loading.
- The Completed Jobs page automatically refreshes, excludes active jobs,
  sorts terminal records newest first, and opens detailed trip cards.
- Added supported trip statistics from SCS configuration, channels, and
  gameplay events. Values not supplied by telemetry remain unavailable.
- Initial window size is 900 x 600 pixels; the minimum remains 900 x 600.
- The installer replaces only NLSI-managed application files and recognized,
  unmodified NLSI plugin DLLs. It checks for ETS2 and ATS before installation
  and never closes either game.

## Telemetry limitations

Some requested metrics are not exposed by the supported SCS SDK callbacks.
These fields intentionally remain `—`, including country names, fuel cost,
and unavailable final statistics. Income is game-reported currency displayed
in the requested EUR format; no exchange-rate conversion is available.

## Data and update behavior

Job records and the sequence are stored under the user's application-data
directory, separate from the installation directory. Updating or uninstalling
the application does not remove these records. The installer does not remove
unrelated or user-modified game plugins.

## Validation

The build pipeline compiles the x64 application/plugin, x86 plugin, runs all
CTest suites, packages the Qt runtime, checks binary architectures and SCS
exports, and compiles the Windows installer. Fresh/update installer execution
is not performed automatically on the developer's machine.
