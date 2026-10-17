# NLSI Exclusive Logbook v1.8.7 Beta

## Job synchronization

- DRIVER, MANAGER, CEO, and ADMIN profiles can now synchronize eligible local
  deliveries assigned to their authenticated driver identity, including jobs
  with no prior per-account sync state.
- Jobs remain pending after network, HTTP, JSON, or acknowledgement failures.
  They are marked synced only after a committed HTTP 200 JSON response confirms
  the authenticated driver and the exact local-to-server job acknowledgements.
- Retry requests retain known server job IDs and rely on the API's existing
  idempotent allocation contract. Jobs already confirmed as synced are not
  uploaded again.

## Monthly task progress contract limitation

The Profile tab presents TASK before RANK. TASK is explicitly shown as
unavailable because the existing desktop API does not expose the monthly task
record. The current `driver-key.php` profile response contains the linked
driver identity, role, rank, and qualifying distance; it has no task month,
target, completed distance, or task status. The API contains no monthly-task
read endpoint. The website's monthly task record/history is protected by its
own logged-in portal and is not available to the desktop client.

Consequently, this release does not fabricate a task percentage, count
unconfirmed local jobs, or alter the website, API, shared environment, or
database schema. The exact missing client contract is an authenticated,
read-only way to retrieve the linked driver's existing `monthly_tasks` data,
including `task_month`, `target_distance_km`, `completed_distance_km`, `status`,
`completed_at_utc`, and `period_finalized_at_utc`, plus the driver's retained
monthly task history needed to display prior periods after rollover. These
columns and website-side history behavior exist in the VTC schema/application,
but are not present in the desktop API response or routes. Until this contract
is exposed, the desktop reports task data as unavailable. Existing
server-calculated rank progress remains unchanged.

## Release contents

- Dashboard: Windows x64, file version `1.8.7.0`, product version `1.8.7 Beta`.
- SCS telemetry plugin: Windows x64 and x86, file version `1.8.7.0`, product
  version `1.8.7 Beta`.
- Installer: version-specific payload under `releases\1.8.7-beta`; user profile
  and local `.nlsi` job records are not packaged or replaced.

This remains a Beta release.
