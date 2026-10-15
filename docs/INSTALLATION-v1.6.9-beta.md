# Installing or updating NLSI Exclusive Logbook 1.6.9 Beta

## Fresh installation

Run `NLSI-Exclusive-Logbook-v1.6.9-beta-Setup.exe`. The default all-users
destination is `C:\Program Files\Nabski Logistics Solutions Inc`; Windows
administrator approval is required. Setup creates the application directory,
Start Menu shortcut, and optional desktop shortcut and installs the dashboard,
Qt runtime, product resources, and both architecture-specific plugin payloads.

When ETS2 or ATS is found in the default or a registered Steam library, Setup
offers its optional plugin task. It installs x64 only under the game's
`bin\win_x64\plugins` directory and x86 only under `bin\win_x86\plugins`.
Missing game/architecture directories are skipped with a message. Running
dashboard or game processes are handled by Setup before file replacement.

## Updating

Run the same installer while the application is installed. The stable product
identifier detects the existing installation and reuses its recorded
installation directory when available. Files in the v1.6.9 payload replace
their installer-managed counterparts. User-created files are not explicitly
deleted.
The installer also removes files recorded by an earlier NLSI payload manifest
when they are no longer part of the new payload; files not listed as
installer-managed are left alone.

An existing plugin is replaced only when its NLSI ownership marker is
recognized and its recorded SHA-256 hash still matches. Setup retains a
pre-update backup for restoration on uninstall. Unmarked or modified plugin
files are not overwritten.

## Uninstalling

Use **NLSI Exclusive Logbook** in Windows Installed Apps / Programs and
Features. Setup removes installer-owned application files and shortcuts. It
does not delete unrelated plugins or user-created files. An unchanged
installer-owned game plugin is restored from backup, or removed if it had no
pre-update backup; a plugin modified after installation is left untouched.

## Steam library discovery

Discovery checks the traditional Steam directory and paths registered in
`steamapps\libraryfolders.vdf`. Non-Steam installations and unregistered Steam
libraries are not modified automatically.
