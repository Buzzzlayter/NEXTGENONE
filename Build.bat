@echo off
rem ===========================================================================
rem  Build.bat
rem  Builds the Development Editor target (Win64) for this UE project, so you
rem  can launch the editor with your latest C++ changes compiled in.
rem
rem  Self-contained: finds the .uproject beside this script, derives the editor
rem  target name from it, and resolves the engine from EngineAssociation via the
rem  registry. No engine path or project name is hard-coded.
rem
rem  NOTE: close the editor first if a real recompile is needed - UBT cannot
rem  overwrite module DLLs while the editor has them loaded.
rem ===========================================================================
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

rem --- 1) locate the .uproject and derive the editor target name -------------
set "UPROJECT="
for %%f in ("%~dp0*.uproject") do set "UPROJECT=%%~ff"
if not defined UPROJECT (
    echo [ERROR] No .uproject found next to this script.
    goto :fail
)
for %%i in ("!UPROJECT!") do set "PROJNAME=%%~ni"
echo [INFO] Project : !UPROJECT!
echo [INFO] Target  : !PROJNAME!Editor Win64 Development

rem --- 2) read EngineAssociation from the .uproject -------------------------
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

rem --- 4) build the editor target ------------------------------------------
set "BUILD_BAT=!ENGINE_ROOT!\Engine\Build\BatchFiles\Build.bat"
if not exist "!BUILD_BAT!" (
    echo [ERROR] Build.bat not found at "!BUILD_BAT!".
    goto :fail
)
echo [INFO] Building...
echo.
call "!BUILD_BAT!" !PROJNAME!Editor Win64 Development -project="!UPROJECT!" -waitmutex
set "ERR=!ERRORLEVEL!"
echo.
if "!ERR!"=="0" (
    echo [OK] Build succeeded.
) else (
    echo [ERROR] Build failed with exit code !ERR!.
)
pause
exit /b !ERR!

:fail
echo.
pause
exit /b 1
