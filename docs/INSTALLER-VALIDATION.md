# Installer validation report

## Summary

Validated the Windows `v1.5.4-beta` build in
`C:\xampp\htdocs\nlsi_telemetry_poc` with Qt 6.12.0 and Visual Studio 2022
MSVC x64. The official build script completed; automated Python and native
tests passed; the setup executable and both SCS and TruckSim plugin
architectures were produced and inspected.

Installer helper tests use temporary Steam-library layouts and temporary DLLs.
The compiled setup executable was **not** launched for a disposable fresh
install or upgrade. Live ETS2/ATS gameplay is **NOT RUN — USER WILL TEST**.
No real game installation or `%LOCALAPPDATA%\NLSI\Exclusive Logbook` data was
modified.

## Build and automated test results

| Check | Result | Evidence |
|---|---|---|
| Official release build | PASS, exit 0 | `& $env:ComSpec /c 'call build-release.bat'` |
| Full Python suite | PASS, 57 tests | Run by `build-release.bat`: `Ran 57 tests`; `OK`. |
| Focused installer integration suite | PASS, 14 tests, exit 0 | `& 'C:\xampp\htdocs\nlsi_telemetry_poc\.venv\Scripts\python.exe' -m unittest discover -s test -p 'test_*installer.py' -v` |
| Native CTest | PASS, 2/2 | Run by `build-release.bat`: `nlsi_core_tests` and `nlsi_gui_tests`; `100% tests passed out of 2`. |
| Setup compilation | PASS | Inno Setup reported `Successful compile`; builder also read the resulting version resource and rejected mismatched metadata. |
| Diff whitespace check | PASS, exit 0 | `git diff --check` |

Focused installer tests compare destination bytes/SHA-256 and PE machine values.
Temporary cases cover:

- ETS2 only, ATS only, and both games; x64 and x86 payload routing.
- Clean install, repeat/idempotent install, and replacement of an older
  installer-managed DLL with different packaged bytes.
- Multiple Steam library roots, while preserving an unmanaged third-party DLL.
- Wrong source architecture, TruckSim source-hash mismatch, missing source,
  missing game, and locked destination failures with status/error evidence.
- SCS plugin backup and restore behavior.
- Preservation of a temporary settings/history fixture during helper runs.

The installer helpers verify post-copy hashes and PE architectures, and
successful cases compare actual destination bytes. A separate injected
post-copy mutation/race and a Windows ACL-denied destination were not simulated;
the locked-file replacement case exercised the failure path without touching a
real game installation.

## Version identity

All current release outputs and the packaged release notes identify as
`1.5.4-beta`. Historical `1.5.3-beta` release notes remain in
`RELEASE_NOTES.md`.

| Artifact | FileVersion | ProductVersion / release label |
|---|---|---|
| `build\releases\v1.5.4-beta\NLSI-Exclusive-Logbook.exe` | `1.5.4.0` | `1.5.4-beta` |
| `build\releases\v1.5.4-beta\NLSI-Exclusive-Logbook-v1.5.4-beta-Setup.exe` | `1.5.4-beta` | `1.5.4.0` |
| Staged release notes | `build\intermediate\installer-payload-v1.5.4-beta\docs\RELEASE_NOTES.md` | Heading `NLSI Exclusive Logbook v1.5.4-beta` |

The setup executable has a numeric product version resource as required for
Windows version metadata; its textual file version, installer `AppVersion`,
filename, release payload, and staged release notes identify the beta label.

## Artifacts and hashes

| Artifact | PE machine | Size | SHA-256 |
|---|---:|---:|---|
| `build\plugins\v1.5.4-beta\win_x64\nlsi.dll` | `0x8664` (x64) | 69,120 bytes | `890F7248FCA85BAE8E1FA742833AEC58876BDC2E5F95F3D717ABE8B14CC367D5` |
| `build\intermediate\installer-payload-v1.5.4-beta\plugins\scs-position\win_x64\nlsi.dll` | `0x8664` (x64) | 69,120 bytes | `890F7248FCA85BAE8E1FA742833AEC58876BDC2E5F95F3D717ABE8B14CC367D5` |
| `build\plugins\v1.5.4-beta\win_x86\nlsi.dll` | `0x014C` (x86) | 58,880 bytes | `95FC6BD4292DBBA707F62B4DE5923509C7D7ACE09C51F9039E7222E1CBE4949B` |
| `build\intermediate\installer-payload-v1.5.4-beta\plugins\scs-position\win_x86\nlsi.dll` | `0x014C` (x86) | 58,880 bytes | `95FC6BD4292DBBA707F62B4DE5923509C7D7ACE09C51F9039E7222E1CBE4949B` |
| `includes\trucksim-gps-plugin\win_x64\plugins\trucksim-gps-telemetry.dll` | `0x8664` (x64) | 167,424 bytes | `4F1A1DD5B879773161C23D657249775D60C9AA362CED171D74C74A16F1AB0F0A` |
| `build\intermediate\installer-payload-v1.5.4-beta\plugins\trucksim\win_x64\plugins\trucksim-gps-telemetry.dll` | `0x8664` (x64) | 167,424 bytes | `4F1A1DD5B879773161C23D657249775D60C9AA362CED171D74C74A16F1AB0F0A` |
| `includes\trucksim-gps-plugin\win_x86\plugins\trucksim-gps-telemetry.dll` | `0x014C` (x86) | 142,336 bytes | `01E5D1CD6AF7C239A7B9E80E9911DE447FC8E80F66EE8F415D859A89F4A403BF` |
| `build\intermediate\installer-payload-v1.5.4-beta\plugins\trucksim\win_x86\plugins\trucksim-gps-telemetry.dll` | `0x014C` (x86) | 142,336 bytes | `01E5D1CD6AF7C239A7B9E80E9911DE447FC8E80F66EE8F415D859A89F4A403BF` |
| `build\releases\v1.5.4-beta\NLSI-Exclusive-Logbook.exe` | `0x8664` (x64) | 1,095,168 bytes | `5F7A1479B3F9BEC2FB22F3C125B726BB8FC62188DEB4B6D6297D8697BB3100F3` |
| `build\releases\v1.5.4-beta\NLSI-Exclusive-Logbook-v1.5.4-beta-Setup.exe` | `0x014C` | 52,761,605 bytes | `35F40FF857ADE73F1BDC0107603A06AF663EA409F7FFAE21FBBA7892466EE4B0` |

For both SCS and TruckSim plugin architectures, the packaged staging copy's
hash matches its version-scoped build output or source payload exactly. The
helper integration tests also verify the correct architecture and copied bytes
at each temporary ETS2/ATS plugin destination.

## Compiled installer execution — NOT RUN

The compiled setup was not run in a disposable Windows installation. The
installer requires interactive Terms and Privacy acceptance, and no isolated
Windows VM or disposable profile was available in this run. Build/package
validation and temporary PowerShell helper tests are not a substitute for
running the compiled installer.

Suggested manual verification in a disposable Windows VM:

1. Snapshot the VM and create temporary Steam library folders containing
   ETS2-only, ATS-only, both-game, and no-game layouts. Install `1.5.4-beta`
   from the setup executable and verify application files and completion status.
2. For each detected game architecture, compare `bin\win_x64\plugins\nlsi.dll`
   and `bin\win_x86\plugins\nlsi.dll` with the matching staged payload by
   SHA-256 and PE machine. Verify unrelated plugin DLLs remain byte-identical.
3. Install a managed `1.5.3-beta` copy, seed disposable per-user configuration,
   history, logs, and exports, then run the `1.5.4-beta` setup over it. Verify
   the existing install path is reused, both managed DLL architectures are
   replaced by the packaged files, user data remains unchanged, and a repeated
   setup run remains safe.
4. Repeat with the game DLL held open by a test process. Verify setup does not
   claim the DLL update succeeded, preserves the prior file, reports an
   actionable failure/status log, and does not terminate the test process.
5. Confirm no-game installation leaves the application installed, records that
   optional plugins were not installed, and does not create spurious game
   plugin directories.

## Live gameplay — NOT RUN — USER WILL TEST

No game was launched. Validate telemetry connection/recovery, provider
independence, session lifecycle, completed-job history, and parking-brake
behavior in ETS2 and ATS during separate live gameplay testing.

## Running-game detection limitation

The installer does not claim to reliably detect whether a game is running.
An open game/plugin DLL may block atomic replacement or removal. The helpers
record the failure and return a failing exit code rather than reporting a
successful plugin update; close the game and retry if that occurs.

No commit, tag, push, or public release was created.
