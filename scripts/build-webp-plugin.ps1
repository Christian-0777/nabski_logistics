param(
    [string]$QtDir = 'C:\Qt\6.12.0\msvc2022_64',
    [string]$VsDevCmd = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat',
    [string]$NinjaPath = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$sourceCommit = 'dc13f2fbff1d579cfb2eb2d9092e3563b4d05b8e'
$archiveHash = 'A433168CE3A55F3E18F801D5387BC84C50E93582E26E8B94ABF744D0839247D2'
$buildCache = Join-Path $projectRoot 'build\qtimageformats-v6.12.0'
$archive = Join-Path $buildCache "qtimageformats-$sourceCommit.zip"
$sourceParent = Join-Path $buildCache 'source'
$sourceRoot = Join-Path $sourceParent "qtimageformats-$sourceCommit"
$pluginBuild = Join-Path $buildCache 'msvc2022-x64'
$pluginRoot = Join-Path $projectRoot 'build\qt-webp-v6.12.0-msvc2022-x64'
$pluginDirectory = Join-Path $pluginRoot 'imageformats'

foreach ($requiredPath in @(
    $QtDir,
    $VsDevCmd,
    $NinjaPath,
    (Join-Path $QtDir 'lib\cmake\Qt6BuildInternals\Qt6BuildInternalsConfig.cmake')
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required Qt/MSVC WebP build input is missing: $requiredPath"
    }
}

New-Item -ItemType Directory -Force -Path $buildCache | Out-Null
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest `
        -Uri "https://github.com/qt/qtimageformats/archive/$sourceCommit.zip" `
        -OutFile $archive -UseBasicParsing
}
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash -ne $archiveHash) {
    throw "The pinned Qt Image Formats source archive failed SHA-256 verification: $archive"
}
if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot 'CMakeLists.txt'))) {
    New-Item -ItemType Directory -Force -Path $sourceParent | Out-Null
    Expand-Archive -LiteralPath $archive -DestinationPath $sourceParent -Force
}
if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot 'src\plugins\imageformats\webp\CMakeLists.txt')) -or
    -not (Test-Path -LiteralPath (Join-Path $sourceRoot 'src\3rdparty\libwebp\COPYING'))) {
    throw "The pinned Qt Image Formats source is incomplete: $sourceRoot"
}

$configureAndBuild = 'cd /d "{0}" && set "PATH={1};%PATH%" && call "{2}" -arch=amd64 -host_arch=amd64 && cmake --fresh -S "{3}" -B "{4}" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="{5}" -DCMAKE_PREFIX_PATH="{1}" -DCMAKE_INSTALL_PREFIX="{6}" -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF -DQT_FEATURE_system_webp=OFF && cmake --build "{4}" --config Release --target QWebpPlugin' -f `
    $projectRoot, (Join-Path $QtDir 'bin'), $VsDevCmd, $sourceRoot, $pluginBuild, $NinjaPath, $pluginRoot
& $env:ComSpec /c $configureAndBuild
if ($LASTEXITCODE -ne 0) {
    throw "The matching Qt 6.12.0 MSVC x64 WebP plugin build failed with exit code $LASTEXITCODE."
}

$builtPlugin = Get-ChildItem -LiteralPath $pluginBuild -Filter 'qwebp.dll' -File -Recurse |
    Select-Object -First 1
if (-not $builtPlugin -or $builtPlugin.Length -eq 0) {
    throw "The Qt Image Formats build did not produce qwebp.dll under $pluginBuild."
}
New-Item -ItemType Directory -Force -Path $pluginDirectory | Out-Null
Copy-Item -LiteralPath $builtPlugin.FullName `
    -Destination (Join-Path $pluginDirectory 'qwebp.dll') -Force
Copy-Item -LiteralPath (Join-Path $sourceRoot 'LICENSES\BSD-3-Clause.txt') `
    -Destination (Join-Path $pluginRoot 'QtImageFormats-BSD-3-Clause.txt') -Force
Copy-Item -LiteralPath (Join-Path $sourceRoot 'src\3rdparty\libwebp\COPYING') `
    -Destination (Join-Path $pluginRoot 'libwebp-COPYING.txt') -Force

Write-Output "WebP plugin: $(Join-Path $pluginDirectory 'qwebp.dll')"
Write-Output "Source: Qt Image Formats 6.12.0, commit $sourceCommit"
