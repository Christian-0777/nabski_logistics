# Installing or updating NLSI Exclusive Logbook 1.7.8 Beta

Run `NLSI-Exclusive-Logbook-v1.7.8-beta-Setup.exe`. The installer retains the
existing product AppId and install location for upgrades. It replaces
application and managed plugin binaries while leaving the per-user encrypted
profile, preferences, local `.nlsi` job files, and synchronization state in
the user's application-data directory.

Close NLSI Exclusive Logbook and ETS2/ATS before installing. If a game plugin
DLL cannot be safely replaced, setup reports the affected path; close the
application using it and run setup again. Unmarked or modified third-party
plugin DLLs are not overwritten.

## Configure the desktop

1. Open **Settings → Config**.
2. Confirm the HTTPS API URL and enter the driver API key issued by the NLSI
   driver portal, then save the configuration.
3. Choose **Test Connection** or refresh **Settings → Profile**. The profile
   is fetched from the account associated with that key and includes the
   server role, rank, and qualifying distance.
4. Jobs synchronize on authenticated connection. Active, pending, delivered,
   and cancelled states are shown in the dashboard/history as available.
   Failed uploads remain local and can be retried on a later connection.
5. Choose a preferred currency in **Settings → Preferences**. ETS2 income is
   natively EUR and ATS income is natively USD. Conversion requires a valid
   Frankfurter reference rate; when the service is unavailable or its rate is
   stale, income remains displayed in the native currency rather than being
   relabelled or converted with a fabricated rate.

An unavailable API does not disable local telemetry, job history,
preferences, or PDF export. Jobs already assigned to another account remain
local and are not automatically transferred to the current account.

## Website/API administrator rollout

This desktop installer does not deploy the website/API or update its
database. Before deploying the matching server-side changes:

1. Back up the database.
2. Apply
   `../../nlsi_vtc/database/migrations/006_admin_roles_and_driver_links.sql`.
3. Deploy the matching API and driver portal files.
4. Use the existing setup flow only for initial ADMIN bootstrap. Public
   administrator registration is disabled; an ADMIN account must create
   additional administrator accounts and assign roles or driver links.

Review the migration's rollback SQL comments before rollback. Schema changes
must be coordinated with the API deployment because the profile endpoint
reads the added role and driver-link columns.
