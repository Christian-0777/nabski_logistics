# Installing or updating NLSI Exclusive Logbook 1.6.7 Beta

## Fresh installation

Run `NLSI-Exclusive-Logbook-v1.6.7-beta-Setup.exe`. The default all-users
destination is `C:\Program Files\Nabski Logistics Solutions Inc` and requires
administrator approval. The installer installs the dashboard, Qt runtime,
platform plugin, product resources, and packaged x64/x86 SCS plugins. It
creates a Start Menu shortcut and offers an optional desktop shortcut.

When a supported Steam game and matching architecture directory are detected,
the installer offers a selected-by-default plugin task. It writes x64 plugins
only to `bin\win_x64\plugins` and x86 plugins only to `bin\win_x86\plugins`.
Missing or unsupported game directories are skipped without preventing the
desktop application from being installed.

An existing unmarked plugin, or a marked NLSI plugin whose recorded hash no
longer matches, is skipped to protect unrelated or changed files. The
application installation can still complete.

## Updating

Run the same installer while NLSI Exclusive Logbook is installed. The stable
product identifier and registered install location allow an in-place update;
uninstalling first is not required. The installer closes the dashboard before
replacing locked application files. Application files supplied by the new
payload are updated, while user-created configuration or records are not
explicitly deleted.

Recognized, unchanged NLSI game plugins are backed up before replacement.
Third-party or changed plugin DLLs are not overwritten.

## Uninstalling

Use **NLSI Exclusive Logbook** in Windows Installed Apps / Programs and
Features. The uninstaller removes installer-owned application files and
shortcuts. It does not delete user-created files or unrelated game plugins.
For a plugin still matching the installed release hash, it restores the
pre-update backup or removes the plugin if no backup exists. A plugin changed
after installation is left untouched.

## Steam library discovery

Discovery checks the traditional Steam directory and paths registered in
`steamapps\libraryfolders.vdf`. Non-Steam installations and unregistered Steam
libraries are not changed automatically.
