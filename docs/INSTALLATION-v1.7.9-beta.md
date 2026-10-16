# Installing or updating NLSI Exclusive Logbook 1.7.9 Beta

Run `NLSI-Exclusive-Logbook-v1.7.9-beta-Setup.exe`. The installer retains the
existing product AppId and install location for upgrades. It replaces
application and managed plugin binaries while leaving the per-user encrypted
profile, preferences, local `.nlsi` job files, and synchronization state in
the user's application-data directory.

Close NLSI Exclusive Logbook and ETS2/ATS before installing. If a game plugin
DLL cannot be safely replaced, setup reports the affected path; close the
application using it and run setup again. Unmarked or modified third-party
plugin DLLs are not overwritten.

## Configure the desktop

1. Open **Settings → Config** and save the HTTPS API URL and driver API key
   issued by the NLSI driver portal.
2. On startup, the application validates the key with `driver-key.php`, then
   fetches server jobs and uploads eligible local completed jobs. The
   **Settings → Profile → Refresh Profile** button repeats profile validation,
   job synchronization, and server-job refresh.
3. Authenticated sessions periodically refresh the remote job list. Failed
   work remains in local storage and retries with bounded backoff; a server
   acknowledgment is required before a local job is marked synced.
4. Check the profile/status and completed-jobs views for distinct upload or
   server-fetch failures. Offline API access does not remove local records or
   disable local telemetry, preferences, or PDF export.

Jobs already assigned to another account remain local and are not
automatically transferred to the currently connected account.

## Website/API rollout

The desktop installer does not deploy the website/API, configure the server
database, or package database credentials. Deploy the updated `nlsi-api`
files to the server; no schema migration is needed when the existing job
sync tables from `db\xtian_db.sql` are installed.
