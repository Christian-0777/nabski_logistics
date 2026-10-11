# NLSI Exclusive Logbook PHP backend

The PHP 8.2/XAMPP backend stores native schema-v2 records in MySQL through PDO
prepared statements. The NLSI VTC website Driver Portal owns driver accounts
and device-key issuance; this backend validates each device key by its SHA-256
hash and derives driver/device identity on the server.

## Version 1.5.7 driver and device setup

Public driver self-signup is disabled. An administrator invites a driver from
`/nlsi_vtc/admin/dashboard.php`; invitations expire after seven days and
activate an account once. First activation creates a primary desktop device
only when no active device exists. Subsequent sign-ins use a separate emailed
six-digit code that expires after ten minutes and is consumed once. New device
keys are displayed once in the authenticated driver portal and are never
emailed. Additional devices, revocation, and replacement remain driver-managed
from `/nlsi_vtc/driver/devices.php`.

Initial administrator setup is a one-time, server-authorized flow at
`/nlsi_vtc/admin/setup.php`. Use it from localhost or through the deployment's
own authenticated HTTPS tunnel; the PHP app rejects unverified forwarded
requests. Setup is protected by CSRF checks, password hashing, and a one-time
database guard. There is no seeded administrator or unrestricted public
administrator self-registration.

Drivers can generate and rotate one 12-character alphanumeric application key
from their authenticated profile. Only a SHA-256 hash is stored in
`driver_access_keys`; rotation revokes the previous value in the same database
transaction. The application validates the key with
`POST /nlsi_vtc/api/driver-key.php` over HTTPS. The request body contains only
`{"key":"..."}` and a successful response contains `valid`, `driver_id`, and
`display_name`. The endpoint does not accept email, username, or password.
The native app keeps this optional key in its own DPAPI-protected
`%LOCALAPPDATA%\NLSI\Exclusive Logbook\sync\driver-key.dpapi` settings file.
It is deliberately separate from telemetry `.nlsi` JSONL files so the
credential cannot invalidate or alter captured telemetry records.

### Applying migration 006 with phpMyAdmin

1. Back up the selected MySQL/MariaDB database.
2. Confirm it already contains the existing `driver_accounts` table and
   migrations 004/005, or was initialized from the consolidated website schema.
3. In phpMyAdmin, select that database, open **Import**, choose
   `C:\xampp\htdocs\nlsi_telemetry_poc\db\migrations\006_driver_access_keys.sql`,
   and run the import. This creates only the new key table and does not modify
   existing driver accounts or telemetry.
4. In the same database's **SQL** tab, run:

   ```sql
   INSERT INTO nlsi_schema_migrations (version, applied_at_utc)
   VALUES ('006_driver_access_keys', UTC_TIMESTAMP(3));
   ```

   This records the phpMyAdmin import so the CLI migration runner will not
   replay it. Fresh installs from
   `C:\xampp\htdocs\nlsi_vtc\database\nlsi_vtc_schema.sql` already create the
   key table and record version 006.

The website requires Composer dependencies (`composer install --no-dev`) and
SMTP settings in its server-only `.env`: `MAIL_HOST`, `MAIL_PORT`,
`MAIL_USERNAME`, `MAIL_PASSWORD`, `MAIL_ENCRYPTION` (`tls` or `smtps`),
`MAIL_FROM_ADDRESS`, and `MAIL_FROM_NAME`. Email delivery must be verified with
the deployment's SMTP provider. Keep the file outside version control and deny
it over HTTP. Never copy database credentials or SMTP secrets to the native
application.

Configure telemetry synchronization separately in API Settings if desired.
The optional driver key is entered through **Enter Your Key** and stored in
`%LOCALAPPDATA%\NLSI\Exclusive Logbook\sync\driver-key.dpapi`, protected with
Windows DPAPI for the current user. Without a key, the application remains
fully usable in LOCAL MODE; DB credentials and server settings stay on the PHP
host. Invalid/revoked keys are distinct from a temporary API outage, and
telemetry records remain local during network failures.

For a fresh install, select/create a database in phpMyAdmin and import only
`C:\xampp\htdocs\nlsi_vtc\database\nlsi_vtc_schema.sql`. It creates tables
only, includes the final state of migrations 001-006, and marks those versions
in the migration ledger. For upgrades, the numbered migrations in
`db/migrations/` remain the source of truth; keep the consolidated fresh
schema synchronized with them. Never run either path without a backup and
operator approval.

Only these values belong in the website `.env`: DB_HOST, DB_PORT, DB_NAME,
DB_USER, DB_PASSWORD, and optional DB_CHARSET, plus optional
DB_MIGRATION_USER/DB_MIGRATION_PASSWORD for the CLI migration runner and
NLSI_SERVER_ENV_FILE only when the protected config file is elsewhere. The
desktop receives only the HTTPS API base URL and its own device key. A prior
shared device credential, if one was used by an earlier deployment, should be
rotated and removed. Current API authorization uses independently issued
per-device keys; no shared API key is configured on the server.

## Legacy deployment configuration (v1.5.6 and earlier)

1. The telemetry API uses the existing server-side
   `C:\xampp\htdocs\nlsi_vtc\.env` when that website configuration is present.
   `lib/Config.php` parses this file directly; the website itself does not load
   it. Set `NLSI_SERVER_ENV_FILE` only when deployment stores configuration at
   another protected path. The required database settings are `DB_HOST`,
   `DB_PORT`, `DB_NAME`, `DB_USER`, and `DB_PASSWORD`; `DB_CHARSET` defaults to
   `utf8mb4`. Give the runtime `DB_USER` only the `SELECT`, `INSERT`, and
   `UPDATE` privileges needed on this schema. Configure
   `DB_MIGRATION_USER` and `DB_MIGRATION_PASSWORD` as a separate CLI-only DDL
   account before applying schema migrations; the runner refuses to reuse
   runtime credentials. Do not expose MySQL port 3306 to the internet or
   through ngrok.
2. This section is retained for historical deployment context only. Current
   provisioning occurs in the authenticated website Driver Portal; it creates
   a separate random key per device and stores only its hash on the server.
3. Keep `.env` out of Git and deny it over HTTP. The website root `.htaccess`
   blocks environment files; `db/.htaccess` also denies example dotenv files,
   PHP configuration, SQL, JSONL, and log files. Apache must allow `.htaccess`
   (`AllowOverride All`). On another web server, add equivalent protections.
4. Back up the database before applying schema changes. For upgrades, run the
   repository migration runner from the root with the XAMPP PHP CLI
   (`php db/run-migrations.php`). It tracks versions in
   `nlsi_schema_migrations`, detects recognized existing schema states, and
   does not drop, reset, truncate, or automatically migrate from the desktop
   app. Do not manually replay a partially applied migration; inspect the
   schema and backup first.
5. Open `/db/dashboard/` on the XAMPP host. Apache restricts the dashboard
   files to local requests by default. For remote operator access, serve it
   only over HTTPS and configure web-server authentication (with its password
   file outside the web root), VPN access, or an equivalent IP/access policy.
   The history, status, and diagnostics PHP endpoints also deny remote access
   unless the trusted web server supplies `REMOTE_USER`; the native `status`
   endpoint is separately protected by the device bearer key. No dashboard
   data is public by default.

To test the native API without revealing a key in a URL or response, set a
temporary local environment variable from the deployment secret store and use
`curl.exe` to send an HTTPS `Authorization: Bearer` header to
`/api/heartbeat.php` below the NLSI VTC site base URL. Check only the HTTP status and JSON database state;
do not paste the key into a URL, dashboard page, source file, command history,
or log, and clear the temporary variable after testing. A missing or invalid
device key must receive HTTP 401.

## API and data safety

* `POST /api/ingest.php` accepts a single JSON record or a batch of up to
  100. Each record is inserted transactionally. It returns explicit
  `acknowledged_record_ids` only after each record commits. Duplicate record
  IDs with identical canonical payload hashes are acknowledged idempotently;
  reused IDs with changed payloads are rejected.
* `POST /api/status.php` accepts up to 500 record IDs and returns server
  acknowledgement state. These native endpoints require the bearer key.
* `GET /api/heartbeat.php` requires the native bearer key and verifies the
  API-to-database connection. The dashboard uses a separate
  `dashboard-status.php` endpoint with the operator access policy.
* `GET /db/api/history.php` supports bounded pagination and an optional exact
  `job_id` filter. Matching job records are ordered by their stable native
  sequence number, not by job name or timestamp. Records without a valid
  provider-supplied job ID remain unassigned.
* `GET /db/api/diagnostics.php` returns counts and sanitized recent error codes
  only to an authorized local/server-authenticated operator.

The native app sends HTTPS requests to PHP; PHP alone connects to MySQL. The
`.nlsi` files and durable local sync queue remain the recoverable source of
records. Failed, partial, timed-out, or ambiguous requests leave unacknowledged
items queued. Session IDs are related only when a matching server session
already exists; ingestion never invents session start times or missing provider
data.

## Migrations and schema

Numbered files in `db/migrations/` are the upgrade source of truth. For fresh
installs, `C:\xampp\htdocs\nlsi_vtc\database\nlsi_vtc_schema.sql` represents
the final state of migrations 001-006 and seeds the same migration ledger.
Keep that table-only schema synchronized when a migration changes. Migration
003 adds nullable, case-sensitive stable `job_id` columns and indexes, and
backfills only job IDs already present in stored native fields. Migration 004
adds driver/device ownership and removes global sequence-number uniqueness
because local sequence counters are independent per device. Migration 005
adds administrator setup, invitations, one-time driver sign-in codes, and
rate-limit/audit storage. Neither path is
automatically applied; operator approval and a backup are required. See
[`docs/SQL-SCHEMA.md`](../docs/SQL-SCHEMA.md) for table mapping and retention.

## Tests

Run `php db/tests/run.php` for environment parsing, access policy, stable
job-ID history filtering, device authentication, invitation activation,
one-time login-code consumption, provisioning/repeat-login behavior,
authentication rate limits, portal source contracts, duplicate idempotency,
session foreign-key behavior, and transaction/acknowledgement tests. These
portal lifecycle tests use in-memory SQLite; they do not prove MySQL migration
or SMTP behavior. Syntax-check PHP on Windows with:

```powershell
Get-ChildItem db -Recurse -Filter *.php | ForEach-Object { php -l $_.FullName }
```

No PHP endpoint test establishes a live MySQL migration or HTTPS/ngrok
end-to-end deployment.
