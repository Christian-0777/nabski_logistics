# NLSI Exclusive Logbook 1.7.8 Beta

## Changes

- Display ATS income as USD and ETS2 income as EUR. When the user's preferred
  currency differs, the application uses a validated EUR/USD reference rate
  from Frankfurter. Rates are refreshed at startup and every six hours, and
  are rejected when invalid or more than ten days old. If no current rate is
  available, the original amount remains in its native denomination.
- Refresh the authenticated driver's server profile, including account role,
  server-calculated rank, and qualifying distance. The server derives rank
  from delivered jobs only:
  - Trainee: below 5,000 km
  - Driver: 5,000 km
  - Elder: 15,000 km
  - Veteran: 25,000 km
  - Expert: 50,000 km
  - Legend: 100,000 km
  - Master: 250,000 km
  - Ultimate: 500,000 km
  - Mythic: 1,000,000 km
- Prefer actual driven distance for rank qualification and use planned
  distance only when actual distance is unavailable. Recognized mile and
  metre units are normalized to kilometres. Legacy `job.delivered` provider
  events remain eligible and are deduplicated by stable job ID.
- Synchronize and display active, pending/unknown, delivered, and cancelled
  jobs. Server state is reconciled into matching local `.nlsi` records without
  clearing populated local values when a response omits them. Local files and
  offline history remain available.
- Add ADMIN-managed account roles and optional admin-to-driver links. Only
  ADMIN can manage administrator accounts and those links; linked role and
  distance rank are shown in the driver profile and portal.

## Server rollout

Back up the database before updating the website/API. Apply
`../../nlsi_vtc/database/migrations/006_admin_roles_and_driver_links.sql`
before deploying the matching profile endpoint, because that endpoint reads
the new role and driver-link fields. The migration has rollback SQL comments;
review those carefully and restore from the backup if rollback is required.
Do not run a rollback after new role or driver-link data has been entered
without first preserving that data.

Public administrator registration is disabled. Preserve the existing
bootstrap setup workflow for the initial ADMIN account; use ADMIN account
management to create additional administrator accounts, assign roles, and
link a driver record. The desktop installer does not apply server migrations
or deploy the website/API.

## Validation scope

Automated PHP and Qt tests, release-build results, live API checks, and live
gameplay checks are reported separately. A local build does not imply that a
production migration, API deployment, live synchronization, or game session
was performed.
