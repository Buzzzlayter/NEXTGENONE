@echo off
rem ===========================================================================
rem  GenerateProjectFiles.bat
rem  Regenerates the Visual Studio solution / project files for this UE project
rem  (same as right-click .uproject -> "Generate Visual Studio project files").
rem
rem  Self-contained: finds the .uproject next to this script and resolves the
rem  engine from the project's EngineAssociation via the registry. No engine
rem  path is hard-coded, so it works across engine versions and machines.
rem ===========================================================================
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

rem --- 1) locate the .uproject beside this script ----------------------------
set "UPROJECT="
for %%f in ("%~dp0*.uproject") do set "UPROJECT=%%~ff"
if not defined UPROJECT (
    echo [ERROR] No .uproject found next to this script.
    goto :fail
)
echo [INFO] Project : !UPROJECT!

rem --- 2) read EngineAssociation from the .uproject --------------------------
set "ASSOC="
for /f "tokens=2 delims=:," %%a in ('findstr /i "EngineAssociation" "!UPROJECT!"') do set "ASSOC=%%a"
set "ASSOC=!ASSOC: =!"
set "ASSOC=!ASSOC:"=!"
if not defined ASSOC (
    echo [ERROR] EngineAssociation is empty - the engine likely sits beside the
    echo         project ^(source build^). Run that engine's GenerateProjectFiles.bat.
    goto :fail
)
echo [INFO] Engine assoc : !ASSOC!

rem --- 3) resolve the engine install dir from the registry ------------------
set "ENGINE_ROOT="
echo !ASSOC! | findstr "{" >nul
if !errorlevel! == 0 (
    rem GUID -> source/custom build registered under ...\Unreal Engine\Builds
    for /f "tokens=2,*" %%a in ('reg query "HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds" /v "!ASSOC!" 2^>nul ^| findstr /i "REG_SZ"') do set "ENGINE_ROOT=%%b"
    if not defined ENGINE_ROOT for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\Epic Games\Unreal Engine\Builds" /v "!ASSOC!" 2^>nul ^| findstr /i "REG_SZ"') do set "ENGINE_ROOT=%%b"
) else (
    rem version (e.g. 5.8) -> launcher-installed engine
    for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\!ASSOC!" /v "InstalledDirectory" 2^>nul ^| findstr /i "REG_SZ"') do set "ENGINE_ROOT=%%b"
    if not defined ENGINE_ROOT for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\!ASSOC!" /v "InstalledDirectory" 2^>nul ^| findstr /i "REG_SZ"') do set "ENGINE_ROOT=%%b"
)
if not defined ENGINE_ROOT (
    echo [ERROR] Could not resolve an engine for EngineAssociation "!ASSOC!".
    goto :fail
)
echo [INFO] Engine root : !ENGINE_ROOT!

rem --- 4) regenerate via UnrealBuildTool (Build.bat wrapper) ----------------
set "BUILD_BAT=!ENGINE_ROOT!\Engine\Build\BatchFiles\Build.bat"
if not exist "!BUILD_BAT!" (
    echo [ERROR] Build.bat not found at "!BUILD_BAT!".
    goto :fail
)
echo [INFO] Regenerating project files...
echo.
call "!BUILD_BAT!" -projectfiles -project="!UPROJECT!" -game -progress
set "ERR=!ERRORLEVEL!"
echo.
if "!ERR!"=="0" (
    echo [OK] Project files regenerated.
) else (
    echo [ERROR] Generation failed with exit code !ERR!.
)
pause
exit /b !ERR!

:fail
echo.
pause
exit /b 1
