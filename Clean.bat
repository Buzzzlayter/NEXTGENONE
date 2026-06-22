@echo off
rem ===========================================================================
rem  Clean.bat
rem  Deletes regenerable build artifacts for this UE project to fix broken
rem  builds / editor crashes and to stop OneDrive from churning on gigabytes
rem  of constantly-changing files.
rem
rem  Removes:   Binaries, Intermediate, DerivedDataCache,
rem             Plugins\*\Binaries, Plugins\*\Intermediate
rem  Keeps:     Content, Config, Source and Saved\ (logs, autosaves) - untouched.
rem
rem  Everything removed is rebuilt automatically: double-click the .uproject
rem  (or run Build.bat) afterwards. NOTE: deleting DerivedDataCache forces a
rem  one-time shader recompile on the next editor launch (can take minutes).
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
echo This will DELETE these regenerable folders under the project:
echo    - Binaries
echo    - Intermediate
echo    - DerivedDataCache   ^(triggers a shader recompile next launch^)
echo    - Plugins\*\Binaries , Plugins\*\Intermediate
echo Content, Config, Source and Saved\ are NOT touched.
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
if exist "%~dp0Plugins\" (
    for /d %%p in ("%~dp0Plugins\*") do (
        call :rmtree "%%~fp\Binaries"
        call :rmtree "%%~fp\Intermediate"
    )
)

echo.
echo [OK] Clean complete. Double-click "!PROJNAME!.uproject" or run Build.bat to rebuild.
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

:fail
echo.
pause
exit /b 1

:end
echo.
pause
exit /b 0
