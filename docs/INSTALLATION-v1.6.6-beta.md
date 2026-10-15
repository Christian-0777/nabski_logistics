# Installing or updating NLSI Exclusive Logbook 1.6.6 Beta

## Fresh installation

1. Run `NLSI-Exclusive-Logbook-v1.6.6-beta-Setup.exe`. Administrator approval
   is required for the default all-users installation.
2. The default application directory is
   `C:\Program Files\Nabski Logistics Solutions Inc\`.
3. Choose whether to add a desktop shortcut.
4. Optionally select ETS2 and/or ATS plugin installation. The installer
   searches the default Steam library and additional libraries listed by the
   Steam `libraryfolders.vdf`. It only offers games for which an installed
   game executable is verified.
5. The installer places a plugin only in an existing architecture directory
   (`bin\win_x64` or `bin\win_x86`) and never puts a 32-bit DLL in an x64
   directory or the reverse. A missing `plugins` directory is created only
   for a selected, verified game architecture.

If a plugin DLL already exists without an NLSI ownership marker, or its
1.6.6-managed hash no longer matches, that target is skipped and reported.
The application can still be installed normally.

## Updating

Run the same installer while the dashboard is installed. The installer closes
the dashboard through Windows Restart Manager before replacing its files,
reuses the existing valid installation location, and updates the existing
uninstall entry. There is no need to uninstall 1.6.5 Beta first. If a previous
release used a different product identifier, uninstall that release through
its own uninstall entry before installing this version.

Files not owned by the installer are preserved. Plugin updates retain an
existing marked NLSI plugin as
`nlsi_scs_plugin.dll.nlsi-backup.dll` in the same game plugin directory.

## Uninstalling

Use **NLSI Exclusive Logbook** in Windows Installed Apps / Programs and
Features. The uninstaller removes installer-owned application files and
shortcuts. It does not delete user-created files from the installation
directory. For a plugin still matching the 1.6.6 installer-written hash, it
restores a retained previous NLSI plugin or removes the plugin installed by
this version. If a plugin has been changed since installation, it is left
untouched; any pre-update backup is retained for manual recovery.

## Steam library limitation

Automatic plugin discovery covers Steam's registered `libraryfolders.vdf`
format and the traditional default Steam directory. A non-Steam copy or a
library not registered in that file is not automatically modified; install
the application without selecting its plugin task and follow the SCS SDK
instructions for that game installation.
