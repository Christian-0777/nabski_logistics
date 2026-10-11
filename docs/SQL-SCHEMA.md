# Telemetry synchronization SQL schema

The backend uses the existing schema-v2 native `.nlsi` JSONL records. Database
upgrade schema changes are kept in the ordered migrations under `db/migrations/`:

1. `001_telemetry_sync.sql` defines the existing session, telemetry, job-record,
   and sync-state tables.
2. `002_preserve_native_records.sql` adds lossless storage for complete native
   records and extends supported sync record types.
3. `003_stable_job_ids.sql` adds nullable, case-sensitive job-ID columns and
   indexed stable-ID queries without fabricating IDs.
4. `004_driver_device_auth.sql` adds website driver accounts, per-device
   SHA-256 key hashes, device audit and rate-limit tables, and authenticated
   driver ownership on new provider records.

For a fresh installation, import the consolidated
`C:\xampp\htdocs\nlsi_vtc\database\nlsi_vtc_schema.sql` after selecting or
creating a database in phpMyAdmin. It creates tables only, contains the final
state of migrations 001-004, and seeds their version entries. Numbered
migrations remain the upgrade source of truth. Back up the database first.
The CLI runner
records versions in `nlsi_schema_migrations`, skips already-applied versions,
and checks for recognized pre-existing schema variations. It does not drop,
truncate, reset, or automatically import data. A partially applied schema
requires operator inspection and an appropriate backup before resuming.

## Tables and source mapping

| Table | Purpose and actual source |
|---|---|
| `driving_sessions` | Trusted session lifecycle only: `session_id`, optional `game_code`, required `started_at_utc`, optional `ended_at_utc` and `end_reason`. Ingestion does not synthesize a session using a telemetry timestamp. |
| `jobs` | Stable, case-sensitive `job_id` primary key. Summary times are populated only from explicit native `job.started`, `job.delivered`, or `job.cancelled` events; absent times and provider fields stay `NULL`. `created_at_utc` and `updated_at_utc` are server-maintained receipt/update times. |
| `telemetry_samples` | Structured TruckSim GPS schema-v2 samples. Includes stable `record_id`, schema version, sequence/capture time, provider/revision, source timestamps, raw fields/availability/mapping, normalized fields, and receive time. |
| `job_records` | Recognized native `job.started`, `job.delivered`, and `job.cancelled` events. Migration 003 adds `job_id`, populated only from a valid native ID; records without one remain unassigned. The earlier nullable `job_identity` field is retained for compatibility. |
| `provider_records` | Added by migration 002. Stores every accepted schema-v2 telemetry sample, provider event, and provider configuration as full `payload_json`, plus identity/index columns and canonical `payload_hash`. Migration 003 adds an indexed nullable `job_id` for exact job history filtering. |
| `sync_state` | Server-side record type, stable ID, acknowledgement state, attempts, retry time, acknowledgement time, sanitized error code, and update time. Ingestion writes `synced` in the same transaction as each accepted record. Referential validation occurs against the actual record table in that transaction. |
| `nlsi_schema_migrations` | Migration runner ledger; records each applied migration version. |
| `driver_accounts` | Website driver login credentials (password hashes), profile name, and active/suspended state. |
| `driver_devices` | Driver-owned device ID, one-way key hash, status and lifecycle timestamps; plaintext keys are not retained. |
| `driver_device_audit` | Audited provision, registration, replacement, and revocation actions. |
| `driver_login_attempts`, `api_auth_attempts` | Hashed login/IP identifiers and request timestamps for rate limiting. |

`provider_records.record_id` and `job_records.record_id` are unique primary
keys. A repeated record ID with identical canonical payload hash is an
acknowledged duplicate; reusing an ID with changed contents is rejected.
New `provider_records` include a nullable `driver_id` resolved from the active
device key by PHP. Legacy records remain unassigned; portal queries scope to
the session-authenticated driver.
Provider sequence numbers are preserved from each local queue but are not
globally unique across devices; stable record UUIDs and canonical fingerprints
provide idempotency. Record type enums match actual native `telemetry_sample`, `provider_event`, and
`provider_configuration` payloads. The structured TruckSim GPS row and
recognized job event are inserted transactionally with the complete provider
record. Job summaries are keyed by the provider's stable ID; their details
contain actual native JSON, while server-maintained receipt timestamps are not
represented as game-reported start times.

Job history is filtered/grouped by the exact case-sensitive `job_id`, then
ordered by native `sequence_no`; job names and timestamps are not identities.
Migration 003 backfills only values present in prior JSON payloads or
`job_identity`. Empty, malformed, or missing IDs remain `NULL`. General
diagnostics and pre-job records remain unassigned.

All `*_utc` timestamps are UTC `DATETIME(3)`. Session foreign keys are populated
only if the referenced session already exists; otherwise the original
`session_id` stays in the complete provider JSON and the relational column is
`NULL`. The native queue does not provide a trusted session-start record, so
ingestion never creates sessions from samples. Existing data is not deleted or
rewritten to complete synchronization.

## API and acknowledgement behavior

Native uploads use HTTPS `POST /api/ingest.php` below the NLSI VTC site base with
`Authorization: Bearer <device key>`. Each incoming record is persisted in a
PDO transaction; the API explicitly acknowledges its stable ID only after
commit. The native client marks local records synced only when the response
confirms the exact requested set of committed record IDs. On failure, timeout,
partial response, or ambiguous outcome, unacknowledged `.nlsi` records and
durable queue entries are retained for retry.

## Rollback and retention

There is no automatic rollback command. Migration 003 is additive but should
not be reversed without a backup and a deliberate retention decision.
Removing `provider_records` or reversing earlier schema migrations can destroy
server-side history. Application upgrades and uninstall must not delete local
`.nlsi` files, job history, logs, or the sync queue.
