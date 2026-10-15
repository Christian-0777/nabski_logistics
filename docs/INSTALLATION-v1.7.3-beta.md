# Installing or updating NLSI Exclusive Logbook 1.7.3 Beta

Run `NLSI-Exclusive-Logbook-v1.7.3-beta-Setup.exe`. Setup retains the stable
product identity and detected previous application directory, so install it
over the existing version; manual uninstall is not required. Close NLSI
Exclusive Logbook and both games before setup.

Setup searches detected Steam libraries for ETS2 and ATS. For each detected
game architecture (x64 and/or x86), it automatically deploys the matching
NLSI plugin DLL. There is no plugin replacement checkbox. Existing plugin
files are replaced only when the NLSI ownership marker and recorded file hash
confirm the DLL is an unchanged NLSI-managed plugin. Unmarked or modified
third-party files are left untouched and reported.

The installer backs up an existing managed plugin before replacement, verifies
the installed DLL against the packaged DLL, and stops with an actionable error
if replacement fails. If a game or other process has a DLL locked, close that
process and rerun setup; setup does not terminate games or claim a failed
replacement succeeded.

The x64 and x86 packaged plugin files are
`plugin\win_x64\nlsi_scs_plugin.dll` and
`plugin\win_x86\nlsi_scs_plugin.dll`. They are installed in each detected
game's matching `bin\win_x64\plugins` or `bin\win_x86\plugins` directory.
Application job history and the job-ID sequence remain in the user's
application-data directory and are not removed by an update.

## Manual install and upgrade verification plan

Use a disposable Windows VM and test Steam-library folders; do not experiment
with production game plugins or saved data.

1. Create test ETS2/ATS folders containing their matching x64 and x86
   executables. For the upgrade case, install the previous NLSI version and
   confirm its managed DLLs and ownership markers exist.
2. Run v1.7.3 over that installation without uninstalling. Confirm Setup
   reuses the application path, replaces application files, deploys both
   matching plugin architectures without a replacement checkbox, updates the
   ownership markers, and preserves a test job-history file.
3. In a separate clean VM, install v1.7.3 with empty plugin directories and
   confirm that every detected architecture receives the matching packaged
   DLL.
4. Repeat with an unmarked third-party DLL and a modified NLSI-marked DLL.
   Confirm neither is replaced and Setup clearly reports the skipped target.
5. Hold a managed test DLL open with a separate process and attempt the update.
   Confirm Setup reports failure and does not report a successful replacement;
   close the process and retry.
6. Confirm the original managed plugin backup can still be restored by
   uninstalling the test application, and verify unrelated game files,
   configuration, logs, and saved trip data remain untouched.

This is a manual verification plan, not a claim that these install scenarios
have been executed.
