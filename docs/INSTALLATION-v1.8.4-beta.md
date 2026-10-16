# NLSI Exclusive Logbook v1.8.4 Beta Installation

Run `NLSI-Exclusive-Logbook-v1.8.4-beta-Setup.exe` and follow the installer
prompts. The installer requires administrator approval to deploy the
application and, when matching game directories are found, the SCS telemetry
plugin files.

Close ETS2 and ATS before installing or upgrading. The installer does not
terminate either game. Plugin DLLs are only replaced when their existing NLSI
ownership marker and content hash validate; unrelated plugin files are left
untouched.

The installer upgrades the application in place while keeping the encrypted
per-user `profile.nlsi`, API credentials, preferences, and local job history in
the user's application-data directory. These files are not included in the
installer payload. Do not manually delete that data directory if you want to
preserve local history.

This release is Beta. Keep independent backups of important local data.
