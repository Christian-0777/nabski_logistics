# Installing or updating NLSI Exclusive Logbook 1.7.7 Beta

Run `NLSI-Exclusive-Logbook-v1.7.7-beta-Setup.exe`. The installer uses the
existing product AppId and install location to support fresh installs and
upgrades. It replaces application and managed plugin binaries while leaving
the per-user encrypted profile, preferences, local `.nlsi` job files, and
local synchronization state in the user's application-data directory.

Close NLSI Exclusive Logbook and ETS2/ATS before installing. If a game plugin
DLL is locked or cannot be safely replaced, setup reports the affected path;
close the application using it and run setup again. Do not manually delete or
replace an unmarked third-party plugin.

## Configure online synchronization

1. Open **Settings → Config**.
2. Confirm the HTTPS API link and enter the driver API key issued by the NLSI
   driver portal, then save the configuration.
3. Choose **Test Connection** or open **Settings → Profile** and refresh the
   account. The profile is loaded from the server account linked to that key.
4. Completed local jobs synchronize after an authenticated connection. Job
   cards show pending, synchronized, or failed state. A failed upload remains
   in the local job history and can be retried on the next connection.

An unavailable API does not disable local telemetry, completed-job history,
preferences, or PDF export. Jobs already assigned to a different account are
kept locally and are not automatically transferred to the currently connected
account.
