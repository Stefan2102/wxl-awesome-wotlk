@echo off
rem Windows build entry point for wxl-awesome-wotlk.
rem
rem Everything is resolved relative to this script, so the repository can live anywhere. Arguments
rem are passed straight through to build.ps1 -- see its help for what it accepts, for example:
rem
rem     build.bat -CorePath ..\wxl-core -ClientPath D:\Path\To\Client
rem     build.bat -Clean -Config Debug

setlocal

set "SCRIPT_DIR=%~dp0"
set "PS_SCRIPT=%SCRIPT_DIR%build.ps1"

if not exist "%PS_SCRIPT%" (
    echo [wxl-awesome-wotlk] build.ps1 was not found next to build.bat.
    echo [wxl-awesome-wotlk] Keep both scripts together in the repository root.
    set "EXITCODE=1"
    goto :finish
)

rem PowerShell 7 if it is installed, Windows PowerShell otherwise. Either satisfies build.ps1.
set "PS_EXE="
where pwsh.exe >nul 2>&1
if not errorlevel 1 set "PS_EXE=pwsh.exe"
if not defined PS_EXE (
    where powershell.exe >nul 2>&1
    if not errorlevel 1 set "PS_EXE=powershell.exe"
)

if not defined PS_EXE (
    echo [wxl-awesome-wotlk] Neither pwsh.exe nor powershell.exe was found on PATH.
    echo [wxl-awesome-wotlk] PowerShell 5.1 or later is required to run build.ps1.
    set "EXITCODE=1"
    goto :finish
)

"%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%PS_SCRIPT%" %*
set "EXITCODE=%ERRORLEVEL%"

:finish
echo.
if "%EXITCODE%"=="0" (
    echo [wxl-awesome-wotlk] BUILD SUCCEEDED.
) else (
    echo [wxl-awesome-wotlk] BUILD FAILED - exit code %EXITCODE%.
)

rem Held open on purpose: when this is launched from Explorer the window would otherwise close
rem before either message could be read.
pause
exit /b %EXITCODE%
