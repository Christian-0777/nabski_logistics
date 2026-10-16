# NLSI Exclusive Logbook v1.8.1 Beta

## Changes

- Add confirmed removal of the local API URL and key. Removal only edits the
  encrypted desktop profile; it does not revoke the server-side key and does
  not remove local jobs, event history, preferences, or other profile fields.
- Keep the shared application header in LOCAL MODE unless the current account
  is authenticated and initialized by the existing API profile endpoint.
- Revalidate the server identity and supported server-defined role before
  account synchronization. Unknown roles, rejected credentials, outages, and
  malformed responses clear active account state.
- Make profile refresh without credentials report "Not connected" without an
  authenticated API request.
- Stop synchronization when the API cannot confirm the account or a complete
  response. Jobs are marked synchronized only after a successful persistence
  acknowledgment for the authenticated driver ID.

## API and local data

The desktop retains the existing `driver-key.php` and `job-sync.php` API
contracts. Supported profile roles are `DRIVER`, `MANAGER`, `CEO`, and `ADMIN`,
as supplied by the API. No website or database schema changes are included.
Authentication and authorization remain server-enforced.

The API key remains encrypted in `profile.nlsi` while configured. Removing it
also clears its active in-memory copy; the stable client instance ID and
unrelated encrypted profile data are retained. Local telemetry and job
history remain available without an API configuration.

## Validation

Automated local validation covers encrypted profile credential removal and
preservation, supported-role validation, the no-configuration profile state,
and the shared LOCAL MODE header. Live authentication for test accounts and
server persistence require a reachable configured API and are not implied by
local test or build results.
