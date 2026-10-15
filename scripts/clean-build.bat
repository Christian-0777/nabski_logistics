@echo off
setlocal enabledelayedexpansion

set ROOT=%~dp0\..
set BUILD_DIR=%ROOT%build
set QT_DIR=C:\Qt\6.12.0\msvc2022_64
set VS_DEV_CMD=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat
set NINJA=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe

if not exist "%QT_DIR%\lib\cmake\Qt6Widgets\Qt6WidgetsConfig.cmake" (
    echo Qt 6.12.0 not found at %QT_DIR%.
    echo Install Qt 6 with MSVC 2022 x64, Widgets, and Network modules.
    exit /b 1
)

if not exist "%NINJA%" (
    echo Ninja was not found at %NINJA%.
    exit /b 1
)

if not exist "%VS_DEV_CMD%" (
    echo Visual Studio 2022 Build Tools was not found at %VS_DEV_CMD%.
    exit /b 1
)

if exist "%BUILD_DIR%" (
    rmdir /s /q "%BUILD_DIR%"
)

mkdir "%BUILD_DIR%"

call "%VS_DEV_CMD%" -arch=amd64 -host_arch=amd64
if errorlevel 1 exit /b %errorlevel%

set "PATH=%QT_DIR%\bin;%PATH%"
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="%NINJA%" -DCMAKE_PREFIX_PATH="%QT_DIR%" -DBUILD_TESTING=ON
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 exit /b %errorlevel%

ctest --test-dir "%BUILD_DIR%" --output-on-failure
if errorlevel 1 exit /b %errorlevel%

echo Clean build complete.
