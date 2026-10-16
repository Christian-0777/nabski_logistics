# Installing or updating NLSI Exclusive Logbook 1.8.0 Beta

Run `NLSI-Exclusive-Logbook-v1.8.0-beta-Setup.exe`. The installer retains the
existing product AppId and install location for upgrades. It replaces
application and managed plugin binaries while leaving the per-user encrypted
profile, preferences, local `.nlsi` job files, and synchronization state in
the user's application-data directory.

Close NLSI Exclusive Logbook and ETS2/ATS before installing. If a game plugin
DLL cannot be safely replaced, setup reports the affected path; close the
application using it and run setup again. Unmarked or modified third-party
plugin DLLs are not overwritten.

## Configure the desktop

1. Open **Settings → Config** and explicitly enter the HTTPS API URL and driver
   API key issued by the NLSI driver portal. Both are stored in the encrypted
   per-user `profile.nlsi`; the key remains masked in the application.
2. **Settings → Profile → Refresh Profile** validates the configured account
   using the existing `driver-key.php` contract, refreshes its server profile
   and rank, and starts the existing safe job synchronization workflow.
3. ONLINE is shown only after the API authenticates the configured account.
   Missing configuration, network/API errors, invalid credentials, and
   malformed responses use LOCAL MODE. Reconnection is checked periodically.
4. Local telemetry and job history remain available in LOCAL MODE. Failed
   synchronization does not delete local records; a local job is marked
   synchronized only after the API confirms persistence.

## Upgrade data

The installer does not replace or remove encrypted profile settings, local job
records, or their synchronization state. It does not install or configure the
website/API, server database, or database credentials.
