# Installing or updating NLSI Exclusive Logbook 1.7.0 Beta

Run `NLSI-Exclusive-Logbook-v1.7.0-beta-Setup.exe`. Administrator approval
may be requested to install under Program Files or deploy a plugin into a
protected game directory. The default application location is
`C:\Program Files\Nabski Logistics Solutions Inc`.

The installer detects the Steam library folders registered on this Windows
account and presents optional ETS2 and ATS plugin tasks only for game
architectures that have a matching executable. Missing games do not prevent
installation of the dashboard. The installed x64 and x86 plugin DLLs are
included in the application directory as architecture-specific payloads.

Before installing or updating, Setup checks for `eurotrucks2.exe` and
`amtrucks.exe`. If either is running, Setup stops with an instruction to close
the game. It never terminates the game. Do not start either game until plugin
replacement is complete.

Updates reuse the prior registered application directory when it exists.
NLSI-managed application files removed from the new payload are cleaned up.
Game plugins are replaced only when the existing ownership marker identifies
an NLSI-installed DLL and its recorded hash still matches. Existing original
NLSI plugin files are backed up before the replacement; unrelated and
user-modified plugins are skipped and reported.

Completed job records, the active-job recovery record, and the persistent job
ID sequence are saved separately in the user's application-data directory as
`.nlsi` files and a sequence file. Updating or uninstalling the application
does not remove this user data. The uninstaller restores a backed-up plugin
only if the current DLL is still the NLSI-managed version.

To run without installing game plugins, leave the optional game plugin tasks
unchecked. You may install the dashboard even if neither game is installed.
