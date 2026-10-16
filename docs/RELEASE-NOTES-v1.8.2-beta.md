# NLSI Exclusive Logbook v1.8.2 Beta

## Changes

- The header now reports `ONLINE` only after the server confirms a valid account profile and supported canonical role. Missing configuration, unreachable APIs, invalid credentials, malformed responses, and unsupported roles leave the app in `LOCAL MODE`.
- Profile and job-sync requests validate HTTP status, JSON content type, JSON syntax, response fields, and server account identity. Jobs are marked synchronized only after the server confirms the committed batch for that account.
- Local job history remains available offline and when changing accounts. Unassigned local jobs are not automatically claimed or uploaded under manager/admin roles; jobs assigned to another account are never uploaded.
- Driver invitations remain single-use and expire after seven days. Driver activation now sets a separate reusable password; legacy active drivers can set or reset it with a single-use emailed code. Signing out does not remove the reusable password.
- Replacing a pending invitation revokes earlier pending codes, creates a new seven-day invitation, and prevents duplicate form submissions. Active driver password credentials are not changed when an administrator sends a one-time sign-in code.
- Application, SCS plugin, and installer release metadata is updated to v1.8.2 Beta.

## Data and compatibility

The Windows installer does not remove or replace local profile settings, completed jobs, event history, or preferences. Server-side driver portals are deployed separately from the desktop installer. Existing `nlsi_vtc` databases require migration `009_driver_login_password.sql`; back up the database and apply it after migrations 005–008. Existing active drivers retain access through the emailed one-time sign-in flow while setting their reusable password.
