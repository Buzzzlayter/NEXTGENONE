@echo off
rem ===========================================================================
rem  RunEditor.bat
rem  Quick-launches the Unreal Editor with this project.
rem
rem  Self-contained: finds the .uproject beside this script and resolves the
rem  engine from EngineAssociation via the registry. No engine path is
rem  hard-coded. Returns immediately (editor opens in its own window).
rem ===========================================================================
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

rem --- 1) locate the .uproject ----------------------------------------------
set "UPROJECT="
for %%f in ("%~dp0*.uproject") do set "UPROJECT=%%~ff"
if not defined UPROJECT (
    echo [ERROR] No .uproject found next to this script.
    goto :fail
)
echo [INFO] Project : !UPROJECT!

rem --- 2) read EngineAssociation --------------------------------------------
set "ASSOC="
for /f "tokens=2 delims=:," %%a in ('findstr /i "EngineAssociation" "!UPROJECT!"') do set "ASSOC=%%a"
set "ASSOC=!ASSOC: =!"
set "ASSOC=!ASSOC:"=!"
if not defined ASSOC (
    echo [ERROR] EngineAssociation is empty ^(source build beside the project?^).
    goto :fail
)

rem --- 3) resolve the engine install dir from the registry ------------------
set "ENGINE_ROOT="
echo !ASSOC! | findstr "{" >nul
if !errorlevel! == 0 (
    for /f "tokens=2,*" %%a in ('reg query "HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds" /v "!ASSOC!" 2^>nul ^| findstr /i "REG_SZ"') do set "ENGINE_ROOT=%%b"
    if not defined ENGINE_ROOT for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\Epic Games\Unreal Engine\Builds" /v "!ASSOC!" 2^>nul ^| findstr /i "REG_SZ"') do set "ENGINE_ROOT=%%b"
) else (
    for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\!ASSOC!" /v "InstalledDirectory" 2^>nul ^| findstr /i "REG_SZ"') do set "ENGINE_ROOT=%%b"
    if not defined ENGINE_ROOT for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\!ASSOC!" /v "InstalledDirectory" 2^>nul ^| findstr /i "REG_SZ"') do set "ENGINE_ROOT=%%b"
)
if not defined ENGINE_ROOT (
    echo [ERROR] Could not resolve an engine for EngineAssociation "!ASSOC!".
    goto :fail
)
echo [INFO] Engine  : !ENGINE_ROOT!

rem --- 4) launch the editor -------------------------------------------------
set "EDITOR=!ENGINE_ROOT!\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "!EDITOR!" (
    echo [ERROR] UnrealEditor.exe not found at "!EDITOR!".
    goto :fail
)
echo [INFO] Launching editor...
start "" "!EDITOR!" "!UPROJECT!"
exit /b 0

:fail
echo.
pause
exit /b 1
