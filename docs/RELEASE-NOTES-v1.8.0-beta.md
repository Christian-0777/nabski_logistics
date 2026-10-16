# NLSI Exclusive Logbook 1.8.0 Beta

## Changes

- Rebuild Settings → Profile around the account header, server-confirmed driver
  rank and qualifying distance, account details, and server synchronization
  status card.
- Keep every profile and rank field explicitly unavailable until the existing
  `driver-key.php` endpoint returns a valid authenticated account and rank.
- Add username, email, and driver ID copy actions plus masked email Show/Hide.
- Show ONLINE only after the configured driver key successfully authenticates.
  Missing configuration, API errors, invalid credentials, and malformed
  profile responses show LOCAL MODE; connection checks retry periodically with
  bounded backoff.
- Remove the built-in API URL default. API URL and key must be configured
  explicitly and remain in encrypted `profile.nlsi` storage.
- Preserve local telemetry, job history, synchronization records, and existing
  settings when offline or upgrading.

## API and data

The desktop uses the existing `driver-key.php` profile response and
`job-sync.php` synchronization workflow. No API route or database schema
changes are included. The desktop release does not include server credentials
or connect directly to MariaDB.

## Validation scope

See the release build output for the actual x64 dashboard and plugin tests,
x86 plugin build, artifact inspection, packaging smoke check, and installer
compilation results. Live API authentication and server persistence require
configured production credentials and are not implied by local build success.
