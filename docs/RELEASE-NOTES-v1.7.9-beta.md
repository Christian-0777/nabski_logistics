# NLSI Exclusive Logbook 1.7.9 Beta

## Changes

- Run the job synchronization pipeline after the configured driver key is
  authenticated at startup and when **Settings → Profile → Refresh Profile**
  is used.
- Periodically check authenticated job synchronization, refresh server jobs,
  and retry failures with a bounded backoff. Concurrent cycles are coalesced;
  a job is marked synced only after a valid committed server acknowledgment.
- Continue attempting local uploads when the initial server-job fetch fails.
  Keep local `.nlsi` files intact and report fetch and upload failures
  separately.
- Reject malformed, non-JSON, unsuccessful, and HTTP-error responses instead
  of treating HTTP 200 alone as a successful profile, job fetch, or upload.
- Roll back failed job batches before returning an error and return generic
  JSON service errors rather than exposing PHP/database diagnostics.

## Database and API

No database schema change is required. The API uses the existing `jobs`,
`provider_records`, `nlsi_job_id_sequence`, and `nlsi_job_id_allocations`
tables. It resolves the owning driver from the authenticated driver key and
commits job and ownership records together before acknowledging an upload.

The API's PHP database configuration continues to load the server-side
`xtian_tools\.env`; desktop binaries and installer payloads do not contain
database credentials. Deploy the updated `nlsi-api` PHP files alongside the
desktop release.

## Validation scope

Build, test, live authenticated synchronization, and clean-install/upgrade
results must be reported separately. A successful local build does not imply
that an authenticated server write or Windows installation was exercised.
