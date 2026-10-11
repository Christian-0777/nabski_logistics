param(
    [Parameter(Mandatory = $true)]
    [string]$DllPath64,

    [Parameter(Mandatory = $true)]
    [string]$DllPath32,

    [Parameter(Mandatory = $true)]
    [string]$ManifestPath,

    [Parameter(Mandatory = $true)]
    [string]$StatusPath,

    [string[]]$SteamLibraryRoots,

    [switch]$RemoveManagedPlugin
)

$ErrorActionPreference = 'Stop'
$expectedHashes = @{
    'win_x64' = '4F1A1DD5B879773161C23D657249775D60C9AA362CED171D74C74A16F1AB0F0A'
    'win_x86' = '01E5D1CD6AF7C239A7B9E80E9911DE447FC8E80F66EE8F415D859A89F4A403BF'
}
$statusDirectory = Split-Path -Parent $StatusPath
$manifestDirectory = Split-Path -Parent $ManifestPath
New-Item -ItemType Directory -Path $statusDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $manifestDirectory -Force | Out-Null

function Write-Status([string]$Message) {
    $line = '{0:u} {1}' -f (Get-Date), $Message
    Add-Content -LiteralPath $StatusPath -Value $line -Encoding UTF8
}

function Get-PeMachine([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $reader = [System.IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) {
            throw "Plugin is not a Windows PE file: $Path"
        }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadUInt32()
        if ($peOffset -gt ($stream.Length - 6)) {
            throw "Plugin has a truncated PE header: $Path"
        }
        $stream.Position = $peOffset
        if ([System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(4)) -ne "PE`0`0") {
            throw "Plugin has an invalid PE signature: $Path"
        }
        return $reader.ReadUInt16()
    }
    finally {
        $stream.Dispose()
    }
}

try {
    $sources = @{
        'win_x64' = (Resolve-Path -LiteralPath $DllPath64 -ErrorAction Stop).Path
        'win_x86' = (Resolve-Path -LiteralPath $DllPath32 -ErrorAction Stop).Path
    }
}
catch {
    $message = "TruckSim GPS source DLL is missing or inaccessible. $($_.Exception.Message)"
    Write-Status "Install: failed. $message"
    throw $message
}
$expectedMachines = @{ win_x64 = 0x8664; win_x86 = 0x014C }
foreach ($architecture in @('win_x64', 'win_x86')) {
    try {
        $source_machine = Get-PeMachine $sources[$architecture]
    }
    catch {
        $message = "Could not read the TruckSim GPS $architecture PE header. $($_.Exception.Message)"
        Write-Status "Install: failed. $message"
        throw $message
    }
    if ($source_machine -ne $expectedMachines[$architecture]) {
        $message = "TruckSim GPS $architecture plugin source has the wrong PE architecture: $($sources[$architecture])"
        Write-Status "Install: failed. $message"
        throw $message
    }
}

function Get-SteamLibraries {
    if ($SteamLibraryRoots -and $SteamLibraryRoots.Count -gt 0) {
        return @($SteamLibraryRoots | Where-Object {
            Test-Path -LiteralPath $_ -PathType Container
        } | ForEach-Object { [System.IO.Path]::GetFullPath($_) })
    }
    $roots = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    foreach ($candidate in @(
        'C:\Program Files (x86)\Steam',
        'C:\Program Files\Steam'
    )) {
        if (Test-Path -LiteralPath $candidate -PathType Container) {
            [void]$roots.Add([System.IO.Path]::GetFullPath($candidate))
        }
    }

    foreach ($registryPath in @(
        'HKLM:\SOFTWARE\Valve\Steam',
        'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam',
        'HKCU:\SOFTWARE\Valve\Steam'
    )) {
        if (Test-Path -LiteralPath $registryPath) {
            $properties = Get-ItemProperty -LiteralPath $registryPath -ErrorAction SilentlyContinue
            foreach ($propertyName in @('InstallPath', 'SteamPath')) {
                $candidate = $properties.$propertyName
                if (-not [string]::IsNullOrWhiteSpace($candidate) -and
                    (Test-Path -LiteralPath $candidate -PathType Container)) {
                    [void]$roots.Add([System.IO.Path]::GetFullPath($candidate))
                }
            }
        }
    }

    foreach ($root in @($roots)) {
        $libraryFolders = Join-Path $root 'steamapps\libraryfolders.vdf'
        if (-not (Test-Path -LiteralPath $libraryFolders -PathType Leaf)) {
            continue
        }
        $content = Get-Content -LiteralPath $libraryFolders -Raw
        foreach ($match in [regex]::Matches($content, '"path"\s+"([^"]+)"')) {
            $library = $match.Groups[1].Value.Replace('\\', '\')
            if (Test-Path -LiteralPath $library -PathType Container) {
                [void]$roots.Add([System.IO.Path]::GetFullPath($library))
            }
        }
    }
    return @($roots)
}

$gameFolders = @(
    'Euro Truck Simulator 2',
    'American Truck Simulator'
)
$pluginName = 'trucksim-gps-telemetry.dll'
$knownDestinations = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
$libraries = @(Get-SteamLibraries)
foreach ($library in $libraries) {
    foreach ($gameFolder in $gameFolders) {
        foreach ($architecture in @('win_x64', 'win_x86')) {
            $pluginDirectory = Join-Path $library "steamapps\common\$gameFolder\bin\$architecture\plugins"
            [void]$knownDestinations.Add([System.IO.Path]::GetFullPath(
                (Join-Path $pluginDirectory $pluginName)))
        }
    }
}

foreach ($architecture in @('win_x64', 'win_x86')) {
    try {
        $actualHash = (Get-FileHash -LiteralPath $sources[$architecture] -Algorithm SHA256).Hash
    }
    catch {
        $message = "Could not calculate TruckSim GPS $architecture source SHA-256. $($_.Exception.Message)"
        Write-Status "Install: failed. $message"
        throw $message
    }
    if ($actualHash -ine $expectedHashes[$architecture]) {
        $message = "TruckSim GPS $architecture plugin hash mismatch: expected $($expectedHashes[$architecture]), got $actualHash."
        Write-Status "Install: failed. $message"
        throw $message
    }
}

if ($RemoveManagedPlugin) {
    if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
        Write-Status 'Uninstall: no managed TruckSim GPS plugin copies were recorded.'
        exit 0
    }
    $uninstallFailures = [System.Collections.Generic.List[string]]::new()
    $managed = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    foreach ($entry in $managed.PSObject.Properties) {
        $destination = [System.IO.Path]::GetFullPath($entry.Name)
        if (-not $knownDestinations.Contains($destination)) {
            Write-Status "Uninstall: ignored unrecognized game plugin path: $destination"
            continue
        }
        if (-not (Test-Path -LiteralPath $destination -PathType Leaf)) {
            Write-Status "Uninstall: already absent: $destination"
            continue
        }
        $actualHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        if ($actualHash -ine [string]$entry.Value) {
            Write-Status "Uninstall: preserved changed or different plugin: $destination"
            continue
        }
        try {
            Remove-Item -LiteralPath $destination -Force
            Write-Status "Uninstall: removed managed plugin: $destination"
        }
        catch {
            $message = "Uninstall: could not remove $destination; close ETS2/ATS and retry. $($_.Exception.Message)"
            $uninstallFailures.Add($message)
            Write-Status $message
        }
    }
    if ($uninstallFailures.Count -gt 0) {
        throw "TruckSim GPS plugin uninstall failed for $($uninstallFailures.Count) destination(s); see $StatusPath."
    }
    exit 0
}

$previousHashes = @{}
if (Test-Path -LiteralPath $ManifestPath -PathType Leaf) {
    try {
        $previous = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
        foreach ($entry in $previous.PSObject.Properties) {
            if (-not [string]::IsNullOrWhiteSpace($entry.Name)) {
                $previousHashes[[System.IO.Path]::GetFullPath($entry.Name)] = [string]$entry.Value
            }
        }
    }
    catch {
        throw "Could not read the existing TruckSim GPS plugin manifest; no existing DLL will be overwritten. $($_.Exception.Message)"
    }
}

$managedHashes = $previousHashes.Clone()
$failures = [System.Collections.Generic.List[string]]::new()
$installed = 0
foreach ($library in $libraries) {
    foreach ($gameFolder in $gameFolders) {
        $gameRoot = Join-Path $library "steamapps\common\$gameFolder"
        if (-not (Test-Path -LiteralPath $gameRoot -PathType Container)) {
            continue
        }
        foreach ($architecture in @('win_x64', 'win_x86')) {
            $architectureRoot = Join-Path $gameRoot "bin\$architecture"
            if (-not (Test-Path -LiteralPath $architectureRoot -PathType Container)) {
                Write-Status "Install: skipped $architecture because the game has no matching architecture directory: $gameRoot"
                continue
            }
            $pluginDirectory = Join-Path $gameRoot "bin\$architecture\plugins"
            $destination = [System.IO.Path]::GetFullPath(
                (Join-Path $pluginDirectory $pluginName))
            try {
                if (Test-Path -LiteralPath $destination -PathType Container) {
                    throw "The TruckSim GPS destination is a directory, not a DLL file: $destination"
                }
                New-Item -ItemType Directory -Path $pluginDirectory -Force | Out-Null
                $sourceHash = $expectedHashes[$architecture]
                $previousHash = $previousHashes[$destination]
                if (Test-Path -LiteralPath $destination -PathType Leaf) {
                    $currentHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
                    if ($currentHash -ieq $sourceHash) {
                        if ($previousHash -ieq $currentHash) {
                            $managedHashes[$destination] = $currentHash
                            Write-Status "Install: already managed TruckSim GPS plugin is current: $destination"
                        }
                        else {
                            Write-Status "Install: preserved an identical plugin not managed by this installer: $destination"
                        }
                        continue
                    }
                    if (-not $previousHash) {
                        $existingMachine = $null
                        try {
                            $existingMachine = Get-PeMachine $destination
                        }
                        catch {
                            Write-Status "Install: preserved an unmanaged non-plugin file at the TruckSim GPS destination: $destination"
                            continue
                        }
                        if ($existingMachine -ne $expectedMachines[$architecture]) {
                            Write-Status "Install: preserved an unmanaged file with a different architecture at the TruckSim GPS destination: $destination"
                            continue
                        }
                    }
                    Write-Status "Install: replacing an outdated TruckSim GPS plugin at $destination"
                }

                $temporary = "$destination.nlsi-$([guid]::NewGuid().ToString('N')).tmp"
                $backup = "$destination.nlsi-$([guid]::NewGuid().ToString('N')).bak"
                try {
                    [System.IO.File]::Copy($sources[$architecture], $temporary, $false)
                    if (Test-Path -LiteralPath $destination -PathType Leaf) {
                        [System.IO.File]::Replace($temporary, $destination, $backup)
                        Write-Status "Install: previous managed plugin preserved for recovery at $backup"
                    }
                    else {
                        [System.IO.File]::Move($temporary, $destination)
                    }
                }
                finally {
                    if (Test-Path -LiteralPath $temporary -PathType Leaf) {
                        Remove-Item -LiteralPath $temporary -Force
                    }
                }
                $installedHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
                if (($installedHash -ine $sourceHash) -or ((Get-PeMachine $destination) -ne $expectedMachines[$architecture])) {
                    throw "Installed plugin hash did not match the verified source: $destination"
                }
                $managedHashes[$destination] = $installedHash
                $installed++
                Write-Status "Install: installed $architecture TruckSim GPS plugin: $destination"
            }
            catch {
                $message = "Install: failed to install TruckSim GPS plugin at $destination. Check that the game folder is writable, close ETS2/ATS and any program using the DLL, then retry. $($_.Exception.Message)"
                $failures.Add($message)
                Write-Status $message
            }
        }
    }
}

$managedHashes | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath $ManifestPath -Encoding UTF8
if ($libraries.Count -eq 0 -or
    ($installed -eq 0 -and $failures.Count -eq 0 -and $managedHashes.Count -eq 0)) {
    Write-Status 'Install: no supported Steam ETS2/ATS plugin destination was found.'
}
if ($failures.Count -gt 0) {
    throw "TruckSim GPS plugin installation failed for $($failures.Count) destination(s); see $StatusPath."
}
if ($installed -eq 0 -and $managedHashes.Count -gt 0) {
    Write-Status 'Install: managed TruckSim GPS plugins are already current.'
}
