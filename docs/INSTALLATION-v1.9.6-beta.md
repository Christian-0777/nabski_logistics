# NLSI Exclusive Logbook v1.9.6 Beta Installation

## Installer and release directory

The built v1.9.6 Beta candidate artifacts are in
`releases\1.9.6-beta`. Run
`NLSI-Exclusive-Logbook-v1.9.6-beta-Setup.exe` from that directory. The
candidate is not identified as published on GitHub by this documentation.

The application is a Windows x64 desktop application. The installer package
also contains x64 and x86 SCS Telemetry plugin DLLs and deploys a plugin only
when the corresponding installed game executable and architecture directory
are found.

## Fresh installation

1. Exit NLSI Exclusive Logbook and ETS2/ATS before installing. Setup aborts
   when a supported game is running; it does not terminate the game.
2. Run the installer, review the application destination, and confirm whether
   matching game installations were detected for plugin deployment.
3. Complete setup, launch NLSI Exclusive Logbook, and then launch the
   supported game to load its telemetry plugin.
4. If telemetry is unavailable, confirm the matching plugin DLL was installed
   in the game's plugin directory and restart the game.

The installer can discover the default Steam library and registered Steam
library folders. For a detected game, it installs only to an existing matching
architecture directory:

| Game | x64 plugin | x86 plugin |
|---|---|---|
| ETS2 | `Euro Truck Simulator 2\bin\win_x64\plugins\nlsi_scs_plugin.dll` | `Euro Truck Simulator 2\bin\win_x86\plugins\nlsi_scs_plugin.dll` |
| ATS | `American Truck Simulator\bin\win_x64\plugins\nlsi_scs_plugin.dll` | `American Truck Simulator\bin\win_x86\plugins\nlsi_scs_plugin.dll` |

These are relative to each game's installation directory.

## Upgrade and existing DLL behavior

The installer retains the existing product AppId and recognizes NLSI-owned
plugin files only when their ownership marker and recorded DLL hash verify.
Unmarked or modified DLLs are not overwritten. Verified NLSI DLLs are backed
up before replacement; the v1.9.6 installer uses `.nlsi-backup` rollback files
and migrates the legacy `.nlsi-backup.dll` form.

This replacement behavior was confirmed in an isolated installer upgrade
test. The test upgraded a staged v1.9.5 installation, replaced marked plugins
in temporary ETS2/ATS x64 and x86 directories, checked the installed DLL hashes,
and verified that user-data sentinels remained byte-for-byte unchanged. It did
not run the production installer against the user's actual game directories.
See `releases\1.9.6-beta\DLL-REPLACEMENT-VERIFICATION.md` for test scope and
limitations.

Per-user preferences, encrypted credentials, completed-job history,
synchronization state, and `.nlsi` records are kept outside the install
payload. The isolated upgrade test verified simulated copies of these data
were preserved. Back up important local data before installing any Beta
release.

Close the dashboard and games before upgrading. Relaunch the game after
installation so it loads the new plugin. A Windows reboot is not documented as
required; reboot-time replacement of a locked DLL was not tested.

## Known limitations

- ETS2/ATS in-game telemetry was not tested for this candidate.
- Plugin installation is offered only for discovered game architectures and
  existing matching plugin directories.
- A locked-DLL replacement across a reboot was not tested.
- This software is **Beta**; compatibility and behavior can change.
