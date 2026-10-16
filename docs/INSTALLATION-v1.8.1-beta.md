# Installing or updating NLSI Exclusive Logbook 1.8.1 Beta

Run `NLSI-Exclusive-Logbook-v1.8.1-beta-Setup.exe`. The installer retains the
existing product AppId and install location for upgrades. It replaces
application and managed plugin binaries while leaving the per-user encrypted
profile, preferences, local `.nlsi` job files, and synchronization state in
the user's application-data directory.

Close NLSI Exclusive Logbook and ETS2/ATS before installing. If a game plugin
DLL cannot be safely replaced, setup reports the affected path; close the
application using it and run setup again. Unmarked or modified third-party
plugin DLLs are not overwritten.

## Configure the desktop

1. Open **Settings → Config** and enter the HTTPS API URL and driver API key
   issued by the NLSI driver portal. Both are stored in the encrypted
   per-user `profile.nlsi`; the key remains masked in the application.
2. **Settings → Profile → Refresh Profile** validates the account using the
   existing `driver-key.php` contract. Account information and its
   server-defined role are displayed only after a valid authenticated response.
3. ONLINE requires successful authentication of the current account. Missing
   configuration, an API outage, rejected credentials, unknown roles, and
   malformed responses use LOCAL MODE.
4. **Remove API Configuration** asks for confirmation and removes the local
   API URL and key only. It does not revoke the server-side key. Local jobs,
   event history, preferences, and profile data remain on the computer.
5. Local telemetry and job history remain available in LOCAL MODE. Local jobs
   are marked synchronized only after server acknowledgment for the
   authenticated account.

## Upgrade data

The installer does not replace or remove encrypted profile settings, local job
records, or their synchronization state. It does not install or configure the
website/API, server database, or database credentials.
