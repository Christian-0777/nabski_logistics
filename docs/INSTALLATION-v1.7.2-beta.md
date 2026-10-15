# Installing or updating NLSI Exclusive Logbook 1.7.2 Beta

Run `NLSI-Exclusive-Logbook-v1.7.2-beta-Setup.exe`. Administrator approval
may be requested to install under Program Files or deploy a plugin into a
protected game directory. The default application location is
`C:\Program Files\Nabski Logistics Solutions Inc`.

The installer detects registered Steam library folders and presents optional
ETS2 and ATS plugin tasks only for installed game architectures with a
matching executable. The x64 and x86 NLSI plugins are included in the
application directory; installing either into a game remains optional.

Before installing or updating, Setup checks for `eurotrucks2.exe` and
`amtrucks.exe`. If either is running, Setup asks you to close the game. It
never terminates a game. Updates reuse the previous application directory
when it exists and remove only obsolete files in the prior NLSI-managed-file
manifest. Existing game plugins are replaced only when their NLSI ownership
marker is recognized and their recorded hash still matches; unrelated and
user-modified files are protected.

Completed job records, the active-job recovery record, and the persistent job
ID sequence remain in the user's application-data directory. Updating or
uninstalling the application does not remove this user data. The uninstaller
restores a backed-up game plugin only while the installed NLSI version is
still unchanged.

Leave the optional game plugin tasks unchecked to install the dashboard
without changing any game installation.
