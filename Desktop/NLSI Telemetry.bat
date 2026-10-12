@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "APP_ROOT=%SCRIPT_DIR%\.."
if "%APP_ROOT:~-1%"=="\" set "APP_ROOT=%APP_ROOT:~0,-1%"

if exist "%APP_ROOT%\agent.py" (
    set "APP_DIR=%APP_ROOT%"
) else if exist "%APP_ROOT%\app\agent.py" (
    set "APP_DIR=%APP_ROOT%\app"
) else (
    echo NLSI Telemetry is not installed correctly.
    echo Expected: "%APP_ROOT%\agent.py" or "%APP_ROOT%\app\agent.py"
    exit /b 1
)

set "PYTHON_EXE=%APP_ROOT%\runtime\pythonw.exe"
set "PYTHON_CONSOLE_EXE=%APP_ROOT%\runtime\python.exe"
pushd "%APP_DIR%"
if exist "%PYTHON_EXE%" goto bundled_runtime
if exist "%PYTHON_CONSOLE_EXE%" goto bundled_console_runtime

where.exe pyw.exe >nul 2>nul
if not errorlevel 1 goto pythonw_runtime
where.exe py.exe >nul 2>nul
if errorlevel 1 goto missing_python

py -3 agent.py
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%

:pythonw_runtime
pyw -3 agent.py
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%

:bundled_runtime
"%PYTHON_EXE%" agent.py
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%

:bundled_console_runtime
"%PYTHON_CONSOLE_EXE%" agent.py
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%

:missing_python
popd
echo Python 3.10 or newer is required to run NLSI Telemetry.
echo Install Python with the Windows Python Launcher, or repair the bundled runtime.
exit /b 1
