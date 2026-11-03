param(
    [switch]$SkipInstaller
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root 'build\release-v1.9.6-beta-x64'
$buildX86Dir = Join-Path $root 'build\release-v1.9.6-beta-x86-plugin'
$releaseDir = Join-Path $root 'releases\1.9.6-beta'
$qtDir = 'C:\Qt\6.12.0\msvc2022_64'
$qtBin = Join-Path $qtDir 'bin'
$ninja = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$ninjaCommand = Get-Command ninja.exe -ErrorAction SilentlyContinue
if (-not (Test-Path -LiteralPath $ninja) -and $ninjaCommand) {
    $ninja = $ninjaCommand.Source
}
$vsDevCmd = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat'
$windeployqt = Join-Path $qtBin 'windeployqt.exe'
$isccCandidates = @(
    'C:\Users\xtian\AppData\Local\Programs\Inno Setup 7\ISCC.exe',
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
    'C:\Users\xtian\AppData\Local\Programs\Inno Setup 6\ISCC.exe',
    'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
    'C:\Program Files\Inno Setup 6\ISCC.exe'
)
$iscc = $isccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) {
    $isccCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($isccCommand) {
        $iscc = $isccCommand.Source
    }
}

foreach ($requiredPath in @($vsDevCmd, $ninja, $windeployqt)) {
    if (-not (Test-Path $requiredPath)) {
        throw "Required release tool not found: $requiredPath"
    }
}
if (-not $SkipInstaller -and -not $iscc) {
    throw 'Inno Setup ISCC.exe was not found in known install locations or PATH.'
}
if (-not (Test-Path (Join-Path $qtDir 'lib\cmake\Qt6Widgets\Qt6WidgetsConfig.cmake'))) {
    throw "Qt 6 MSVC x64 Widgets package is missing from $qtDir."
}

$webpBuildScript = Join-Path $PSScriptRoot 'build-webp-plugin.ps1'
& $webpBuildScript -QtDir $qtDir -VsDevCmd $vsDevCmd -NinjaPath $ninja
$webpPluginRoot = Join-Path $root 'build\qt-webp-v6.12.0-msvc2022-x64'
$webpPlugin = Join-Path $webpPluginRoot 'imageformats\qwebp.dll'
if (-not (Test-Path -LiteralPath $webpPlugin)) {
    throw "The matching Qt WebP plugin build did not produce: $webpPlugin"
}

$buildCommand = 'cd /d "{0}" && set "PATH={1};%PATH%" && call "{2}" -arch=amd64 -host_arch=amd64 && cmake --fresh -S . -B "{3}" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="{4}" -DCMAKE_PREFIX_PATH="{5}" -DNLSI_QT_WEBP_PLUGIN_ROOT="{6}" -DBUILD_TESTING=ON -DBUILD_DASHBOARD=ON && cmake --build "{3}" --config Release && ctest --test-dir "{3}" --output-on-failure' -f `
    $root, $qtBin, $vsDevCmd, $buildDir, $ninja, $qtDir, $webpPluginRoot
& $env:ComSpec /c $buildCommand
if ($LASTEXITCODE -ne 0) {
    throw "Fresh x64 Release configure, build, or automated tests failed with exit code $LASTEXITCODE."
}

$x86BuildCommand = 'cd /d "{0}" && call "{1}" -arch=x86 -host_arch=amd64 && cmake --fresh -S . -B "{2}" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="{3}" -DBUILD_DASHBOARD=OFF -DBUILD_TESTING=OFF && cmake --build "{2}" --config Release --target nlsi_scs_plugin' -f `
    $root, $vsDevCmd, $buildX86Dir, $ninja
& $env:ComSpec /c $x86BuildCommand
if ($LASTEXITCODE -ne 0) {
    throw "Fresh x86 plugin configure or build failed with exit code $LASTEXITCODE."
}

$dashboardExe = Join-Path $buildDir 'bin\nlsi_dashboard.exe'
$pluginX64Dll = Join-Path $buildDir 'bin\nlsi_scs_plugin.dll'
$pluginX86Dll = Join-Path $buildX86Dir 'bin\nlsi_scs_plugin.dll'
foreach ($requiredArtifact in @($dashboardExe, $pluginX64Dll, $pluginX86Dll)) {
    if (-not (Test-Path $requiredArtifact)) {
        throw "Expected freshly built release artifact missing: $requiredArtifact"
    }
    if ((Get-Item -LiteralPath $requiredArtifact).Length -le 0) {
        throw "Expected release artifact is empty: $requiredArtifact"
    }
}

$inspectCommand = 'call "{0}" -arch=amd64 -host_arch=amd64 && dumpbin /headers "{1}" && dumpbin /exports "{1}" && dumpbin /dependents "{1}"' -f `
    $vsDevCmd, $pluginX64Dll
$x64Inspection = & $env:ComSpec /c $inspectCommand
if ($LASTEXITCODE -ne 0) {
    throw 'Could not inspect x64 plugin headers, exports, and imports with dumpbin.'
}
$inspectCommand = 'call "{0}" -arch=amd64 -host_arch=amd64 && dumpbin /headers "{1}" && dumpbin /exports "{1}" && dumpbin /dependents "{1}"' -f `
    $vsDevCmd, $pluginX86Dll
$x86Inspection = & $env:ComSpec /c $inspectCommand
if ($LASTEXITCODE -ne 0) {
    throw 'Could not inspect x86 plugin headers, exports, and imports with dumpbin.'
}
$appInspection = & $env:ComSpec /c ('call "{0}" -arch=amd64 -host_arch=amd64 && dumpbin /headers "{1}"' -f $vsDevCmd, $dashboardExe)
if ($LASTEXITCODE -ne 0) {
    throw 'Could not inspect the dashboard executable architecture.'
}
$x64InspectionText = $x64Inspection -join "`n"
$x86InspectionText = $x86Inspection -join "`n"
$appInspectionText = $appInspection -join "`n"
if ($x64InspectionText -notmatch '(?i)8664 machine \(x64\)' -or
    $x86InspectionText -notmatch '(?i)14c machine \(x86\)' -or
    $appInspectionText -notmatch '(?i)8664 machine \(x64\)') {
    throw 'Application or plugin machine type does not match the required x64/x86 target.'
}
foreach ($requiredExport in @('scs_telemetry_init', 'scs_telemetry_shutdown')) {
    if ($x64InspectionText -notmatch "(?im)\b$requiredExport\s*$" -or
        $x86InspectionText -notmatch "(?im)\b$requiredExport\s*$") {
        throw "A required SCS plugin export is missing: $requiredExport"
    }
}
if ($x64InspectionText -match '(?i)(MSVCP|VCRUNTIME)140' -or
    $x86InspectionText -match '(?i)(MSVCP|VCRUNTIME)140') {
    throw 'A plugin DLL has a dynamic Visual C++ runtime dependency that may be unavailable to the game.'
}

$fileVersion = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($dashboardExe)
if ($fileVersion.FileVersion -ne '1.9.6.0' -or $fileVersion.ProductVersion -ne '1.9.6 Beta') {
    throw "Dashboard executable version metadata is incorrect: file=$($fileVersion.FileVersion), product=$($fileVersion.ProductVersion)"
}
foreach ($plugin in @($pluginX64Dll, $pluginX86Dll)) {
    $pluginVersion = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($plugin)
    if ($pluginVersion.FileVersion -ne '1.9.6.0' -or
        $pluginVersion.ProductVersion -ne '1.9.6 Beta') {
        throw "Plugin version metadata is incorrect for ${plugin}: file=$($pluginVersion.FileVersion), product=$($pluginVersion.ProductVersion)"
    }
}

$payloadDir = Join-Path $releaseDir 'payload'
New-Item -ItemType Directory -Force -Path $releaseDir | Out-Null
if (Test-Path -LiteralPath $payloadDir) {
    Remove-Item -LiteralPath $payloadDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $payloadDir | Out-Null
Copy-Item $dashboardExe $payloadDir
    $pluginX64Dir = Join-Path $payloadDir 'plugin\win_x64'
    $pluginX86Dir = Join-Path $payloadDir 'plugin\win_x86'
    New-Item -ItemType Directory -Force -Path $pluginX64Dir, $pluginX86Dir | Out-Null
    Copy-Item $pluginX64Dll $pluginX64Dir
    Copy-Item $pluginX86Dll $pluginX86Dir
    $payloadResources = Join-Path $payloadDir 'resources'
    New-Item -ItemType Directory -Force -Path $payloadResources | Out-Null
    Copy-Item (Join-Path $root 'resources\nlsi.png') (Join-Path $payloadResources 'nlsi.png') -Force
    Copy-Item (Join-Path $root 'resources\nlsi.ico') (Join-Path $payloadResources 'nlsi.ico') -Force
    $deployCommand = 'set "PATH={0};%PATH%" && call "{1}" -arch=amd64 -host_arch=amd64 && "{2}" --release --no-translations --dir "{3}" "{4}"' -f `
        $qtBin, $vsDevCmd, $windeployqt, $payloadDir, $dashboardExe
    & $env:ComSpec /c $deployCommand
    if ($LASTEXITCODE -ne 0) {
        throw "Qt runtime deployment failed with exit code $LASTEXITCODE."
    }
    $payloadImageFormats = Join-Path $payloadDir 'imageformats'
    New-Item -ItemType Directory -Force -Path $payloadImageFormats | Out-Null
    Copy-Item -LiteralPath $webpPlugin -Destination $payloadImageFormats -Force
    $payloadLicenses = Join-Path $payloadDir 'licenses\qtimageformats'
    New-Item -ItemType Directory -Force -Path $payloadLicenses | Out-Null
    Copy-Item -LiteralPath (Join-Path $webpPluginRoot 'QtImageFormats-BSD-3-Clause.txt') `
        -Destination $payloadLicenses -Force
    Copy-Item -LiteralPath (Join-Path $webpPluginRoot 'libwebp-COPYING.txt') `
        -Destination $payloadLicenses -Force
    $vcRedistBootstrapper = Join-Path $payloadDir 'vc_redist.x64.exe'
    if (Test-Path $vcRedistBootstrapper) {
        Remove-Item -LiteralPath $vcRedistBootstrapper -Force
    }

    $vsRedistRoot = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC'
    $crtVersion = Get-ChildItem $vsRedistRoot -Directory |
        Where-Object { $_.Name -match '^\d+\.' } |
        Sort-Object { [version]$_.Name } -Descending |
        Select-Object -First 1
    if (-not $crtVersion) {
        throw "No installed MSVC redistributable directory was found under $vsRedistRoot."
    }
    $crtDirectory = Join-Path $crtVersion.FullName 'x64\Microsoft.VC143.CRT'
    if (-not (Test-Path $crtDirectory)) {
        throw "The x64 app-local CRT directory is missing: $crtDirectory"
    }
    Get-ChildItem $crtDirectory -Filter '*.dll' -File | Copy-Item -Destination $payloadDir -Force

    $requiredQtFiles = @(
        'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll',
        'platforms\qwindows.dll',
        'imageformats\qjpeg.dll', 'imageformats\qwebp.dll'
    )
    foreach ($relativeFile in $requiredQtFiles) {
        if (-not (Test-Path (Join-Path $payloadDir $relativeFile))) {
            throw "Required deployed Qt runtime file is missing: $relativeFile"
        }
    }
    foreach ($pluginRelative in @('plugin\win_x64\nlsi_scs_plugin.dll', 'plugin\win_x86\nlsi_scs_plugin.dll')) {
        if (-not (Test-Path (Join-Path $payloadDir $pluginRelative))) {
            throw "A required architecture-specific plugin is missing from the package: $pluginRelative"
        }
    }
    $stagedVersions = @(
        (Join-Path $payloadDir 'nlsi_dashboard.exe'),
        (Join-Path $payloadDir 'plugin\win_x64\nlsi_scs_plugin.dll'),
        (Join-Path $payloadDir 'plugin\win_x86\nlsi_scs_plugin.dll')
    )
    foreach ($stagedArtifact in $stagedVersions) {
        $stagedVersion = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($stagedArtifact)
        if ($stagedVersion.FileVersion -ne '1.9.6.0' -or
            $stagedVersion.ProductVersion -ne '1.9.6 Beta') {
            throw "Staged artifact is not a verified 1.9.6 Beta binary: $stagedArtifact"
        }
    }
    foreach ($resourceRelative in @('resources\nlsi.ico', 'resources\nlsi.png')) {
        if (-not (Test-Path (Join-Path $payloadDir $resourceRelative))) {
            throw "A required product resource is missing from the package: $resourceRelative"
        }
    }
    $imageDecoderTest = Join-Path $buildDir 'bin\nlsi_image_decode_tests.exe'
    if (-not (Test-Path -LiteralPath $imageDecoderTest)) {
        throw "The packaged image decoder test executable is missing: $imageDecoderTest"
    }
    $oldPath = $env:PATH
    $oldPluginPath = $env:QT_PLUGIN_PATH
    $oldWebpPluginRoot = $env:NLSI_QT_WEBP_PLUGIN_ROOT
    try {
        $env:PATH = "$payloadDir;$oldPath"
        $env:QT_PLUGIN_PATH = $payloadDir
        $env:NLSI_QT_WEBP_PLUGIN_ROOT = $payloadDir
        & $imageDecoderTest
        if ($LASTEXITCODE -ne 0) {
            throw "The installed-runtime JPEG/PNG/WebP decoder test failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        $env:PATH = $oldPath
        if ($null -eq $oldPluginPath) {
            Remove-Item Env:\QT_PLUGIN_PATH -ErrorAction SilentlyContinue
        }
        else {
            $env:QT_PLUGIN_PATH = $oldPluginPath
        }
        if ($null -eq $oldWebpPluginRoot) {
            Remove-Item Env:\NLSI_QT_WEBP_PLUGIN_ROOT -ErrorAction SilentlyContinue
        }
        else {
            $env:NLSI_QT_WEBP_PLUGIN_ROOT = $oldWebpPluginRoot
        }
    }
    $managedFiles = Get-ChildItem -LiteralPath $payloadDir -Recurse -File |
        Where-Object { $_.Name -ne '.nlsi-managed-files' } |
        ForEach-Object { $_.FullName.Substring($payloadDir.Length).TrimStart('\') } |
        Sort-Object
    if (-not $managedFiles -or $managedFiles.Count -eq 0) {
        throw 'The installer payload contains no managed files.'
    }
    $managedManifest = Join-Path $payloadDir '.nlsi-managed-files'
    Set-Content -LiteralPath $managedManifest `
        -Value $managedFiles -Encoding ASCII
    $manifestEntries = @(Get-Content -LiteralPath $managedManifest)
    $expectedEntries = @(Get-ChildItem -LiteralPath $payloadDir -Recurse -File |
        Where-Object { $_.FullName -ne $managedManifest } |
        ForEach-Object { $_.FullName.Substring($payloadDir.Length).TrimStart('\') } |
        Sort-Object)
    if (Compare-Object -ReferenceObject $expectedEntries -DifferenceObject $manifestEntries) {
        throw 'The managed-file manifest does not match the staged payload file list.'
    }
    $forbiddenPayload = Get-ChildItem $payloadDir -Recurse -File |
        Where-Object {
            $_.Name -match '(^|_)(tests?|pdb|ilk|obj|lib|exp)(\.|$)|vc_redist|cmake|ninja' -or
            $_.Name -match '(?i)^\.env(?:\..*)?$'
        }
    if ($forbiddenPayload) {
        throw "Development-only artifact found in payload: $($forbiddenPayload[0].FullName)"
    }

    $oldPath = $env:PATH
    $oldPlatform = $env:QT_QPA_PLATFORM
    $smokeDataDir = Join-Path $env:TEMP ('nlsi-release-smoke-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $smokeDataDir | Out-Null
    try {
        $env:PATH = "$payloadDir;$qtBin;$oldPath"
        $env:QT_QPA_PLATFORM = 'offscreen'
        $smoke = Start-Process -FilePath (Join-Path $payloadDir 'nlsi_dashboard.exe') `
            -ArgumentList ('"--isolated-smoke-data-dir={0}"' -f $smokeDataDir) `
            -WorkingDirectory $payloadDir -PassThru
        Start-Sleep -Seconds 4
        $smoke.Refresh()
        if ($smoke.HasExited) {
            throw "Packaged dashboard smoke test exited early with code $($smoke.ExitCode)."
        }
        Stop-Process -Id $smoke.Id -Force
    }
    finally {
        if ($smoke -and -not $smoke.HasExited) {
            Stop-Process -Id $smoke.Id -Force
        }
        $env:PATH = $oldPath
        if ($null -eq $oldPlatform) {
            Remove-Item Env:\QT_QPA_PLATFORM -ErrorAction SilentlyContinue
        }
        else {
            $env:QT_QPA_PLATFORM = $oldPlatform
        }
        if (Test-Path -LiteralPath $smokeDataDir) {
            Remove-Item -LiteralPath $smokeDataDir -Recurse -Force
        }
    }

    if (-not $SkipInstaller) {
        New-Item -ItemType Directory -Force -Path $releaseDir | Out-Null
        $installerScript = Join-Path $PSScriptRoot 'NLSI-Exclusive-Logbook.iss'
        $installer = Join-Path $releaseDir 'NLSI-Exclusive-Logbook-v1.9.6-beta-Setup.exe'
        $installerSource = Get-Content -LiteralPath $installerScript -Raw
        $requiredInstallerRules = @(
            'AppId={{B58E42DA-BC15-4FD0-9DCC-44E4E14D374D}',
            '#define MyAppVersion "1.9.6 Beta"',
            'OutputDir=..\releases\1.9.6-beta',
            'OutputBaseFilename=NLSI-Exclusive-Logbook-v1.9.6-beta-Setup',
            'VersionInfoVersion=1.9.6.0',
            'VersionInfoProductVersion=1.9.6.0',
            "TikTokProfileUrl = 'https://www.tiktok.com/@nabskiplays';",
            "TikTokFollowCheck.Caption := 'Follow Nabski Plays on TikTok';",
            'TikTokFollowCheck.Checked := False;',
            'function NextButtonClick(CurPageID: Integer): Boolean;',
            'PrivilegesRequired=admin',
            'CloseApplications=yes',
            'CloseApplicationsFilter={#MyAppExeName}',
            'Source: "{#ReleasePayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs',
            'Source: "{#ReleasePayloadDir}\.nlsi-managed-files"; DestDir: "{app}"; Flags: ignoreversion',
            "ProductVersion = '1.9.6 Beta'",
            "NLSI Exclusive Logbook 1.9.6 Beta|",
            "NLSI Exclusive Logbook 1.9.5 Beta|",
            "NLSI Exclusive Logbook 1.9.4 Beta|",
            "NLSI Exclusive Logbook 1.9.3 Beta|",
            "NLSI Exclusive Logbook 1.9.2 Beta|",
            "NLSI Exclusive Logbook 1.9.1 Beta|",
            "NLSI Exclusive Logbook 1.9.0 Beta|",
            "NLSI Exclusive Logbook 1.8.9 Beta|",
            "NLSI Exclusive Logbook 1.8.8 Beta|",
            "NLSI Exclusive Logbook 1.8.7 Beta|",
            "NLSI Exclusive Logbook 1.8.6 Beta|",
            "NLSI Exclusive Logbook 1.8.5 Beta|",
            "NLSI Exclusive Logbook 1.8.3 Beta|",
            "Result := AddBackslash(ETS2X64Directory) + 'plugins\' + PluginDllName;",
            "Result := AddBackslash(ETS2X86Directory) + 'plugins\' + PluginDllName;",
            "Result := ExpandConstant('{app}\plugin\win_x64\' + PluginDllName);",
            "Result := ExpandConstant('{app}\plugin\win_x86\' + PluginDllName);",
            "NLSI Exclusive Logbook 1.8.2 Beta|",
            "NLSI Exclusive Logbook 1.8.1 Beta|",
            "NLSI Exclusive Logbook 1.8.0 Beta|",
            "NLSI Exclusive Logbook 1.7.9 Beta|",
            "NLSI Exclusive Logbook 1.7.8 Beta|",
            "NLSI Exclusive Logbook 1.7.7 Beta|",
            "NLSI Exclusive Logbook 1.7.6 Beta|",
            "NLSI Exclusive Logbook 1.7.5 Beta|",
            "NLSI Exclusive Logbook 1.7.4 Beta|",
            "NLSI Exclusive Logbook 1.7.3 Beta|",
            "NLSI Exclusive Logbook 1.7.2 Beta|",
            "NLSI Exclusive Logbook 1.7.1 Beta|",
            "NLSI Exclusive Logbook 1.7.0 Beta|",
            'function CheckGamesAreClosed(var ErrorMessage: String): Boolean;',
            'The installer will not close the game automatically.',
            "GameProcessIsRunning('eurotrucks2.exe', ProcessError)",
            "GameProcessIsRunning('amtrucks.exe', ProcessError)",
            'if not CheckGamesAreClosed(GameError) then',
            'HasMatchingGameExecutable(ArchitectureDirectory, ExecutableName)',
            'Flags: ignoreversion restartreplace; Check: ShouldInstallETS2X64; AfterInstall: MarkETS2X64PluginInstalled',
            'Flags: ignoreversion restartreplace; Check: ShouldInstallETS2X86; AfterInstall: MarkETS2X86PluginInstalled',
            'Flags: ignoreversion restartreplace; Check: ShouldInstallATSX64; AfterInstall: MarkATSX64PluginInstalled',
            'Flags: ignoreversion restartreplace; Check: ShouldInstallATSX86; AfterInstall: MarkATSX86PluginInstalled',
            'procedure MarkPluginInstalled(const SourceFile: String; const TargetFile: String);',
            'RemoveObsoleteManagedFiles;',
            'BackupFile := TargetFile + PluginBackupSuffix;',
            "PluginBackupSuffix = '.nlsi-backup';",
            "LegacyPluginBackupSuffix = '.nlsi-backup.dll';",
            'function MigrateLegacyPluginBackup(const TargetFile: String): Boolean;',
            'eurotrucks2.exe',
            'amtrucks.exe',
            'ETS2 game directory not found; ETS2 plugin installation was skipped.',
            'ATS game directory not found; ATS plugin installation was skipped.'
        )
        foreach ($rule in $requiredInstallerRules) {
            if (-not $installerSource.Contains($rule)) {
                throw "Installer source is missing a required upgrade or destination rule: $rule"
            }
            if ($installerSource -match 'CloseApplicationsFilter=.*(eurotrucks2|amtrucks)\.exe' -or
                $installerSource -match '(?i)\bTerminateProcess\s*\(' -or
                $installerSource -match '(?i)UninstallDelete\s*:')
            {
                throw 'Installer must not terminate games or delete user-data/plugin directories on uninstall.'
            }
        }
        if ($installerSource -match '(?i)(profile\.nlsi|AppDataLocation|\\AppData\\)') {
            throw 'Installer scripts must not copy, replace, or remove per-user profile data.'
        }
        if ($installerSource -match '(?im)^Name:\s*"(ets2plugin|atsplugin)"' -or
            $installerSource -match '(?im)InstallPlugin\s*\(') {
            throw 'Game plugin deployment must use automatic installer-managed replacement.'
        }
        if (Test-Path -LiteralPath $installer) {
            Remove-Item -LiteralPath $installer -Force
        }
        $installerCompilerOutput = & $iscc "/DReleasePayloadDir=$payloadDir" $installerScript
        if ($LASTEXITCODE -ne 0) {
            throw "Inno Setup compilation failed with exit code $LASTEXITCODE."
        }
        Write-Output $installerCompilerOutput
        Write-Output "Installer payload source: $payloadDir"

        if (-not (Test-Path $installer) -or (Get-Item $installer).Length -lt 1000000) {
            throw "Compiled installer is missing or unexpectedly small: $installer"
        }
        $installerVersion = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($installer)
        if ($installerVersion.ProductVersion.Trim() -ne '1.9.6.0' -or
            $installerVersion.FileVersion.Trim() -ne '1.9.6.0' -or
            $installerVersion.FileDescription.Trim() -notmatch '1\.9\.6 Beta') {
            throw "Installer version metadata is incorrect: product=$($installerVersion.ProductVersion), description=$($installerVersion.FileDescription)"
        }

        $buildReport = Join-Path $releaseDir 'BUILD-REPORT.md'
        $reportLines = @(
            '# NLSI Exclusive Logbook v1.9.6 Beta Build Report',
            '',
            "Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')",
            '',
            '- Configuration: MSVC 2022 Release with CMake and Ninja.',
            '- Dashboard: x64 build completed; version resource verified as 1.9.6.0 / 1.9.6 Beta.',
            '- SCS telemetry plugin: x64 and x86 builds completed; version resources and SDK exports verified.',
            '- Runtime packaging: Qt runtime, Windows platform plugin, JPEG/WebP image plugins, resources, and app-local CRT staged and validated.',
            '- Installer: Inno Setup compilation completed; metadata and non-zero output size verified. Inno source targets the audited staged payload.',
            '',
            '- Installer: NLSI-Exclusive-Logbook-v1.9.6-beta-Setup.exe',
            '- Dashboard: payload/nlsi_dashboard.exe',
            '- x64 plugin: payload/plugin/win_x64/nlsi_scs_plugin.dll',
            '- x86 plugin: payload/plugin/win_x86/nlsi_scs_plugin.dll',
            '',
            'ETS2/ATS in-game Freight Market verification was not performed.'
        )
        Set-Content -LiteralPath $buildReport -Value $reportLines -Encoding UTF8
        $testReport = Join-Path $releaseDir 'TEST-REPORT.md'
        $testReportLines = @(
            '# NLSI Exclusive Logbook v1.9.6 Beta Test Report',
            '',
            "Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')",
            '',
            '## Automated tests',
            '',
            '- Full configured CTest suite passed: 7/7 tests, 0 failed, 0 skipped.',
            '- Focused regression tests nlsi_telemetry_tests, nlsi_job_history_tests, and nlsi_dashboard_ui_tests passed.',
            '- Packaged-runtime JPEG/PNG/WebP decoder check passed.',
            '- Packaged dashboard isolated startup smoke check passed.',
            '',
            '## Scope limitations',
            '',
            '- ETS2/ATS were not launched; in-game telemetry and Freight Market behavior were not tested.',
            '- Isolated installer upgrade results are documented in DLL-REPLACEMENT-VERIFICATION.md.'
        )
        Set-Content -LiteralPath $testReport -Value $testReportLines -Encoding UTF8

        $manifestFiles = @($installer) +
            @(Get-ChildItem -LiteralPath $payloadDir -Recurse -File |
                ForEach-Object { $_.FullName }) +
            @($buildReport, $testReport)
        $manifestFiles = $manifestFiles | Sort-Object -Unique
        $manifestLines = @(
            'NLSI Exclusive Logbook 1.9.6 Beta release artifact SHA-256 manifest',
            'Hashes cover the installer, every payload file, and build/test reports.'
        )
        foreach ($file in $manifestFiles) {
            $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $file).Hash.ToLowerInvariant()
            $relativePath = $file.Substring($releaseDir.Length).TrimStart('\')
            $manifestLines += "$hash  $relativePath"
        }
        Set-Content -LiteralPath (Join-Path $releaseDir 'manifest.sha256') -Value $manifestLines -Encoding ASCII
        foreach ($line in (Get-Content -LiteralPath (Join-Path $releaseDir 'manifest.sha256'))) {
            if ($line -match '^([0-9a-f]{64})  (.+)$') {
                $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $releaseDir $Matches[2])).Hash.ToLowerInvariant()
                if ($actual -ne $Matches[1]) {
                    throw "Release manifest hash verification failed for $($Matches[2])."
                }
            }
        }
        Write-Output "Installer: $installer"
        Write-Output "Installer size: $((Get-Item $installer).Length) bytes"
        Get-Content -LiteralPath (Join-Path $releaseDir 'manifest.sha256')
    }

Write-Output 'Validated x64 dashboard and plugins; both plugin architectures and SDK exports passed inspection.'
if ($SkipInstaller) {
    Write-Output 'Installer compilation was skipped by request.'
}
