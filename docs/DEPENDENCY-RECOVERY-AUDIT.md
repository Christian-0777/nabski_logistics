# NLSI Exclusive Logbook Dependency Recovery Audit

**Audit date:** 2026-10-13 (local time, UTC+08:00)  
**Project path:** `C:\xampp\htdocs\xtian_tools\projects\nlsi-exclusive\nlsi_telemetry_poc`  
**Target:** NLSI Exclusive Logbook **v1.6.3 Beta**  
**Method:** Read-only inspection. No build, test suite, installer, package install, download, or system configuration change was run.

## Executive summary

The backup contains the expected v1.6.3 Beta source, documentation, bundled SDKs, version-specific CMake caches, application and plugin binaries, an installer, and a staged installer payload. The system has the documented Windows x64, Visual Studio 2022/MSVC, Windows SDK, CMake, Qt 6.12, and Inno Setup dependencies. No third-party Python package manifest or Python package dependency is needed for the documented Python compatibility tests.

**The one confirmed build-pipeline blocker at audit time was the project `.venv`:** its launcher targeted Python 3.14.7 under the previous Windows account `C:\Users\kuyac\...`, which does not exist. With that stale launcher present, `build-installer.bat` would select it instead of falling back to `py -3`.

No software or SDK download appears necessary. Existing v1.6.3 Beta artifacts have matching version metadata, but their timestamps predate the latest commit, so they are not proof that those binaries were built from that exact commit.

**Approved follow-up after the read-only audit:** the old environment was preserved as `.venv.pre-reset`, and a fresh `.venv` was created with the already-installed Python 3.12.10. Its interpreter and pip work; pip is the only installed package. No source files, system PATH, build outputs, or user data were changed. Tests and builds remain unrun.

## 1. Backup and repository verification

| Check | Result |
|---|---|
| Project directory | FOUND |
| Git root | Project directory |
| Branch and status before writing this report | `main`, tracking `origin/main`; clean |
| Latest commit | `4bcb51f`, 2026-10-12 21:54:39 +08:00, `NLSI Exclusive Logbook v1.6.3 Beta` |
| Version metadata | `version.json` specifies version `1.6.3`, channel `beta`; unchanged |
| Critical files | README, [DEVELOPMENT.md](./DEVELOPMENT.md), CMake files, release scripts, native source, tests, installer scripts/definition, release notes, and version metadata are present |
| CMake source references | 72 literal source references checked across the native and SCS plugin CMake files; none missing |
| Active installer inputs | Required images, notices, plugin binaries, SDK/license files, and installer scripts checked; none missing |

Backup observations:

- The root `.env` is **zero bytes, ignored, and untracked**. Its contents were not read. `.env.example` is tracked. No secret contents were displayed or copied.
- `.gitignore` excludes `.env`, `.venv`, `build`, and common credential/key files. Generated build outputs and the virtual environment are present locally but ignored, rather than tracked.
- A tracked, zero-byte `installer\prepare_installer_background.ps1` exists and has no repository references. It is not used by the inspected build scripts; confirm its status against the original backup if that file was expected to contain code.
- Several unusual tracked top-level filenames resemble detached diff/README fragments (`e NLSI Exclusive Logbook v1.5.9 Beta…`, `how c314429…README.md`, and `tatus`). They are not referenced by the build scripts. They were left untouched.
- Numerous versioned and historical build/cache directories are included. Do not clean them as part of recovery. The current-version CMake caches are distinct from older caches.

## 2. Installed tools and configuration

| Tool or component | Detected version and location | Status |
|---|---|---|
| Windows | Windows 10 Pro 22H2, build 19045, 64-bit x64 | FOUND |
| Git | 2.56.0, `C:\Program Files\Git\cmd\git.exe` | FOUND |
| Python launcher / system Python | `py -3` resolves to Python 3.12.10 at `C:\Users\xtian\AppData\Local\Programs\Python\Python312\python.exe`; standard-library and Tkinter import smoke check passed | FOUND |
| pip for system Python | 25.0.1; the global package list contains pip only | FOUND |
| Original project `.venv` | Its `pyvenv.cfg` recorded Python 3.14.7 and the missing base executable `C:\Users\kuyac\AppData\Local\Python\pythoncore-3.14-64\python.exe`; preserved as `.venv.pre-reset` | MISCONFIGURED at audit; preserved |
| Current project `.venv` | Python 3.12.10, pip 25.0.1; no additional packages installed | FOUND; interpreter and pip verified |
| CMake | 4.4.4 at `C:\Program Files\CMake\bin\cmake.exe`; exceeds documented 3.20 minimum | FOUND |
| Visual Studio Build Tools / MSVC | VS 2022 Build Tools found by `vswhere`; MSVC x64 compiler 19.44.35229 (toolset 14.44.35207); `cl /Bv` succeeded in the VS developer environment | FOUND |
| Windows SDK | 10.0.26100.0 headers and x64 libraries present under `C:\Program Files (x86)\Windows Kits\10` | FOUND |
| Qt | Qt 6.12.0 MSVC 2022 x64 at `C:\Qt\6.12.0\msvc2022_64`; Qt6 CMake configuration for Widgets, Network, Svg, and Concurrent, runtime DLLs, and `windeployqt` found; `windeployqt --version` returned 6.12.0 | FOUND |
| CMake/Qt current build cache | `build\cmake-v1.6.3-beta\CMakeCache.txt` records Visual Studio 17 2022, x64, this project root, and Qt6 at the installation above | FOUND |
| CMake/Qt environment | `CMAKE_PREFIX_PATH` and `Qt6_DIR` are not set in the environment. The release script has a matching `C:\Qt\6.12.0\msvc2022_64` fallback, and the current-version cache already records Qt6 | OPTIONAL; no current blocker |
| Inno Setup | Inno Setup 6 `ISCC.exe` found at `C:\Users\xtian\AppData\Local\Programs\Inno Setup 6\ISCC.exe`; command-line help ran successfully. Exact patch version was not reported by the executable metadata/help output | FOUND |
| 7-Zip | 26.04 at `C:\Program Files\7-Zip\7z.exe`; not on PATH and not referenced by the inspected active build/release scripts | OPTIONAL |
| XAMPP / PHP | PHP 8.2.12 at `C:\xampp\php\php.exe`; XAMPP Apache and MySQL executables are present | OPTIONAL for local PHP backend development; not required for native app, plugin, Python tests, or installer build |
| Composer | 2.10.3 at `C:\ProgramData\ComposerSetup\bin\composer.bat`; no Composer manifest is present | OPTIONAL for backend work; not required by the native build |
| PATH resolution | Git, Python, `py`, pip, CMake, PHP, and Composer are discoverable. `cl`, `ISCC`, `7z`, and Qt tools are not all on PATH; project scripts use Visual Studio discovery, explicit installer candidates, the Qt CMake cache, or a documented Qt path instead | FOUND with script-managed discovery |
| MSVC runtime / redistributables | MSVC 14.44.35211 x64 CRT files and x64/x86 `vc_redist` installers are present in the Visual Studio Build Tools tree | FOUND |

## 3. Project dependencies and scope

| Dependency | Required for | Audit result |
|---|---|---|
| Windows x64, Visual Studio 2022 C++ workload/MSVC, Windows SDK, CMake 3.20+ | Native Qt application and x64/x86 SCS position plugins | All found; compiler and Windows SDK files checked |
| Qt 6.12 for MSVC 2022 x64: Widgets, Network, Svg, Concurrent | Native application, GUI tests, and installer runtime staging | All four module configurations and required Qt deployment tool found |
| SCS Telemetry SDK 1.15 | SCS position plugin and legacy plugin target | Repository-bundled at `includes\scs_sdk_1_15`; 46 files found, including the telemetry header and license. The native plugin CMake configuration uses this repository-local include directory. `build.bat` also defaults to this path; an external SDK copy is not required. |
| Python 3.10+ and `py` launcher | Reference implementation and documented Python compatibility tests | System Python 3.12.10 and a working `py -3` are present. Python code imports standard-library modules; no `requirements.txt`, `pyproject.toml`, setup manifest, or third-party Python runtime package list exists. |
| Tkinter | Reference GUI and Python GUI tests | Available in Python 3.12.10 (Tk 8.6). |
| Inno Setup 6 / `ISCC.exe` | Native installer compilation | Found and command-line usable. |
| Visual C++ 14.3 runtime files and x86/x64 redistributable installers | Installer runtime packaging | Found in the MSVC 14.44.35112 redist tree; existing installer payload also contains the expected CRT DLLs and redistributable installers. |
| PHP 8.2, XAMPP, MySQL, Composer | Optional PHP sync-backend development/deployment | XAMPP PHP, Apache, MySQL and Composer are present. No Composer manifest exists. These are not local native application build prerequisites. |
| 7-Zip | None in the inspected active build path | Optional; installed but unused by the project scripts checked. |
| Production credentials / `.env` values | Not required to compile the native application, plugins, run local unit tests, or produce a local installer | Not required for local build. Do not add production secrets to perform recovery. |

The supported native release path is `build-release.bat` → CMake/MSVC native application and plugin builds → Python compatibility tests and CTest → `build-installer.bat` → `installer\build_native_installer.py` and Inno Setup. The older `installer\build_installer.py` is a separate legacy path: it requires an SDK argument and expects the legacy `build\v<VERSION>\nlsi_telemetry.dll` output. It is not the native installer path called by the current batch scripts.

## 4. Environment variables and path assumptions

Only variable **names** are recorded here; values were not read or reported.

| Names | Purpose and build relevance |
|---|---|
| `CMAKE_PREFIX_PATH`, `Qt6_DIR` | Qt/CMake discovery. Neither is set globally. The release script’s documented Qt 6.12 fallback exists, and the current release cache points to it. For manual CMake configuration, pass `-DCMAKE_PREFIX_PATH="C:\Qt\6.12.0\msvc2022_64"` on that command rather than changing system PATH. |
| `NLSI_API_URL`, `NLSI_WEBSITE_URL`, `NLSI_UPDATE_URL`, `SOCIAL_TIKTOK_URL`, `SOCIAL_DISCORD_URL`, `SOCIAL_FACEBOOK_URL`, `SOCIAL_YOUTUBE_URL` | Names in the tracked root `.env.example` for optional/reference GUI configuration; not native build secrets or compile prerequisites. |
| `DB_HOST`, `DB_PORT`, `DB_NAME`, `DB_USER`, `DB_PASSWORD`, `DB_CHARSET`, `NLSI_SERVER_ENV_FILE` | PHP backend configuration names. `NLSI_SERVER_ENV_FILE` is an optional alternate server environment-file path, not a local native build requirement. |
| `NLSI_DISABLE_UPDATE_CHECK` | Optional test/runtime flag used by native CTest configuration; not a build prerequisite. |
| `SCS_SDK_ROOT` | Not set. The inspected active build scripts do not require it: the current plugin CMake file uses the bundled SDK, and `build.bat` defaults to `includes\scs_sdk_1_15`. |

`cmake-build\CMakeCache.txt` is stale for recovery: it records `Qt6_DIR-NOTFOUND` and uses the `native` subdirectory as its CMake source root. Do not use it for this release. The version-specific `build\cmake-v1.6.3-beta` cache is configured with the correct generator, architecture, Qt installation, and repository root.

## 5. Existing v1.6.3 Beta artifacts

| Artifact | Observed size / modified time | Version evidence and result |
|---|---:|---|
| Native application: `build\releases\v1.6.3-beta\NLSI-Exclusive-Logbook.exe` | 1,280,000 bytes; 2026-10-12 20:11:12 | Product `NLSI Exclusive Logbook`, ProductVersion `1.6.3-beta`, FileVersion `1.6.3.0`; FOUND |
| SCS position plugin x64: `build\plugins\v1.6.3-beta\win_x64\nlsi.dll` | 69,120 bytes; 2026-10-12 20:11:29 | FOUND in target-version directory; no file-version resource, so version is evidenced by path, not embedded metadata |
| SCS position plugin x86: `build\plugins\v1.6.3-beta\win_x86\nlsi.dll` | 58,880 bytes; 2026-10-12 20:18:56 | FOUND in target-version directory; no file-version resource, so version is evidenced by path, not embedded metadata |
| Native installer: `build\releases\v1.6.3-beta\NLSI-Exclusive-Logbook-v1.6.3-beta-Setup.exe` | 52,812,786 bytes; 2026-10-12 20:37:36 | Product `NLSI Exclusive Logbook`, ProductVersion `1.6.3.0`, FileVersion `1.6.3-beta`; FOUND |
| Native installer payload: `build\intermediate\installer-payload-v1.6.3-beta` | 52 files, 76,199,101 bytes | Expected application, Qt modules including Concurrent, Qt platform/TLS plugins, CRT DLLs, and both SCS position plugins found |
| MSVC redistributable payload: `build\intermediate\installer-redist-v1.6.3-beta` | 2 files, 39,589,160 bytes | Both x64 and x86 redistributable installers found |
| Python agent/runtime | `agent.py` and `gui_app.py` are source files at project root; no Python executable/runtime is expected in the native release directory | The active native app/installer is not the reference Python application; Python is not a native runtime dependency |

The application, plugin, installer, and current-version build caches have timestamps earlier than the latest commit timestamp. Their version metadata agrees with v1.6.3 Beta, but the audit did not prove binary/source correspondence to commit `4bcb51f`. The standalone application output directory does not contain `Qt6Concurrent.dll` or the MSVC CRT DLLs; the existing installer payload does contain them. Standalone launchability of the unpackaged output was not tested.

## 6. Buildability assessment

No build or test command was executed during this audit.

| Target | Assessment |
|---|---|
| Native application | **EXPECTED BUILDABLE; NOT VERIFIED BY BUILD.** Required CMake/MSVC/SDK/Qt components and a matching version-specific CMake cache are present. |
| SCS position plugin (x64/x86) | **EXPECTED BUILDABLE; NOT VERIFIED BY BUILD.** MSVC, Windows SDK, plugin source, and repository-local SCS SDK are present. Both target-directory DLLs already exist. |
| Python compatibility tests | **EXPECTED RUNNABLE; NOT VERIFIED BY TEST RUN.** `py -3` is usable with Python 3.12.10, Tkinter and standard-library imports work, and the test command uses `unittest`. No third-party Python dependencies are declared. |
| Native CTest targets | **EXPECTED BUILDABLE; NOT VERIFIED BY BUILD/TEST.** Qt modules, MSVC, and the current-version CMake cache are present. |
| Existing v1.6.3 Beta installer | **FOUND AND VERSION-VERIFIED** by file presence, nonzero size, and Windows file metadata. |
| Installer rebuild through `build-installer.bat` / full release batch | **EXPECTED BUILDABLE AFTER VENV REPAIR; NOT VERIFIED.** The wrapper now finds a working Python 3.12 virtual environment. The installer and release commands were not run. |
| Native installer helper | **PREREQUISITES FOUND; NOT RUN.** The current CMake cache, `windeployqt`, Qt runtime, MSVC redistributables, Inno Setup, release executable, package assets, and versioned payload locations are present. |

## 7. Prioritized recovery checklist

1. **[Complete] Repair the stale virtual environment without deleting it.** With approval, the ignored original `.venv` was renamed to `.venv.pre-reset` and a fresh environment was created using the already-installed interpreter:

   ```powershell
   py -3.12 -m venv .venv
   ```

   The fresh interpreter and pip were verified. The project has no Python dependency manifest and the inspected Python code uses the standard library, so no other packages were installed.

2. **Use the current-version CMake cache and documented Qt path.** Avoid `cmake-build` and historical version caches. If reconfiguring manually, use `build\cmake-v1.6.3-beta`, Visual Studio 17 2022, x64, and `C:\Qt\6.12.0\msvc2022_64`. Do not change system PATH; project scripts already locate Visual Studio, Qt deployment tools, and Inno Setup.

3. **Keep existing user data and outputs.** Do not clean the repository or remove `.nlsi` records, delivery history, configuration, sync queues, ignored build directories, or the existing v1.6.3 Beta package. A rebuild targets version-specific output locations that already contain artifacts; get approval before running release scripts that may replace those outputs or regenerate installer staging.

4. **After approval, validate in increasing scope:** run the documented Python `unittest` command with `py -3`, then native CMake/CTest, then the release/installer pipeline. Do not install PHP, Composer, 7-Zip, another Qt version, or another SCS SDK for the native recovery unless a later command produces evidence that one is needed.

## Audit conclusion

The extracted source and required native build dependencies are substantially intact. No required compiler, Qt module, SDK, or installer compiler is missing. The stale `.venv` that blocked the batch-driven installer/full-release workflow was preserved and replaced with a working Python 3.12 environment after approval. Existing target-version artifacts are present, but no rebuild or tests were run. No system-level change, third-party package installation, source change, or output cleanup was performed.
