@echo off
rem ===========================================================================
rem  DeepClean.bat
rem  Aggressive "reset to a clean state" for this UE project: removes every
rem  regenerable build / IDE / Saved artifact. Use it to fix broken builds or
rem  editor crashes, or to shrink the working tree before a backup.
rem
rem  Removes:   Binaries, Intermediate, DerivedDataCache, Saved,
rem             Plugins\*\Binaries, Plugins\*\Intermediate,
rem             *.sln, *.slnx  (solution files in the project root)
rem  Keeps:     Content, Config, Source - untouched.
rem
rem  Everything removed is regenerated automatically:
rem    - GenerateProjectFiles.bat               -> .sln / .slnx
rem    - double-click .uproject or run Build.bat -> Binaries / Intermediate
rem    - first editor launch rebuilds DerivedDataCache (shader recompile, slow)
rem      and recreates Saved (logs, autosaves; editor layout resets to default).
rem ===========================================================================
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

rem --- 1) locate the .uproject (sanity: we are in a project root) ------------
set "UPROJECT="
for %%f in ("%~dp0*.uproject") do set "UPROJECT=%%~ff"
if not defined UPROJECT (
    echo [ERROR] No .uproject found next to this script.
    goto :fail
)
for %%i in ("!UPROJECT!") do set "PROJNAME=%%~ni"
echo [INFO] Project : !UPROJECT!

rem --- 2) refuse to run while the editor is open ----------------------------
tasklist /fi "imagename eq UnrealEditor.exe" 2>nul | findstr /i "UnrealEditor.exe" >nul
if !errorlevel! == 0 (
    echo [ERROR] UnrealEditor.exe is running - close the editor first, then retry.
    goto :fail
)

rem --- 3) confirm before deleting -------------------------------------------
echo.
echo This will DELETE these regenerable items under the project:
echo    - Binaries
echo    - Intermediate
echo    - DerivedDataCache   ^(triggers a shader recompile next launch^)
echo    - Saved              ^(logs, autosaves; editor layout resets to default^)
echo    - Plugins\*\Binaries , Plugins\*\Intermediate
echo    - *.sln , *.slnx     ^(regenerate with GenerateProjectFiles.bat^)
echo Content, Config and Source are NOT touched.
echo.
set "CONFIRM="
set /p "CONFIRM=Proceed? [y/N] "
if /i not "!CONFIRM!"=="y" (
    echo [INFO] Cancelled - nothing was deleted.
    goto :end
)
echo.

rem --- 4) delete -------------------------------------------------------------
call :rmtree "%~dp0Binaries"
call :rmtree "%~dp0Intermediate"
call :rmtree "%~dp0DerivedDataCache"
call :rmtree "%~dp0Saved"
if exist "%~dp0Plugins\" (
    for /d %%p in ("%~dp0Plugins\*") do (
        call :rmtree "%%~fp\Binaries"
        call :rmtree "%%~fp\Intermediate"
    )
)
rem cwd is the project root (cd above), so these wildcards match root only.
for %%f in (*.sln *.slnx) do call :rmfile "%%~ff"

echo.
echo [OK] Clean complete. Run GenerateProjectFiles.bat, then double-click
echo      "!PROJNAME!.uproject" or run Build.bat to rebuild.
goto :end

rem --------------------------------------------------------------------------
:rmtree
if exist "%~1" (
    echo [INFO] Removing %~1
    rmdir /s /q "%~1"
    if exist "%~1" echo [WARN] Could not fully remove %~1 ^(a file may be in use^).
) else (
    echo [INFO] Skip ^(not present^): %~1
)
exit /b 0

:rmfile
if exist "%~1" (
    echo [INFO] Removing %~1
    del /f /q "%~1"
    if exist "%~1" echo [WARN] Could not remove %~1 ^(a file may be in use^).
)
exit /b 0

:fail
echo.
pause
exit /b 1

:end
echo.
pause
exit /b 0
