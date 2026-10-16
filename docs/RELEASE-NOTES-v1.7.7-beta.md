# NLSI Exclusive Logbook 1.7.7 Beta — Online Synchronization

## What's new

- Load and validate the saved driver API key at startup and refresh the actual
  account profile from the existing `driver-key.php` API.
- Fetch completed server jobs and reconcile them with local `.nlsi` records
  without duplicate cards.
- Synchronize local completed jobs using the authenticated driver's ownership,
  a stable encrypted-profile client ID, and the existing server-side job ID
  allocation and provider-record tables.
- Keep per-job pending, synchronized, or failed state in a separate local
  state file. Local job files and Summary event history are never replaced by
  server responses or removed after a failed or successful connection.
- Preserve local data for offline use and prevent jobs already assigned to one
  account from being uploaded to a different authenticated account.
- Keep unit preferences, historical job details, PDF export, and the existing
  telemetry plugin integration.
- Align the application, x64/x86 telemetry DLLs, installer, and release
  metadata to 1.7.7 Beta.

## API

The desktop uses the existing HTTPS API base URL and 12-character driver key.
Profile validation uses `POST driver-key.php`; online job listing and upload
use the authenticated `GET|POST job-sync.php` route. The server associates
requests with the driver identified by the hashed API key, not a client-
supplied account ID.

## Verification

Release builds and test results are reported separately from live API and
gameplay checks. A successful package build does not claim that a live game
session or production synchronization was performed.
