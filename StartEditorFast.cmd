@echo off
setlocal
set "NG_ENGINE=S:\UE_5.8\Engine"
set "NG_PROJECT=%~dp0NEXTGENONE.uproject"
call "%NG_ENGINE%\Build\BatchFiles\Build.bat" NEXTGENONEEditor Win64 Development -Project="%NG_PROJECT%" -WaitMutex
set "NG_BUILD_RESULT=%ERRORLEVEL%"
if not "%NG_BUILD_RESULT%"=="0" (
    echo Build failed with exit code %NG_BUILD_RESULT%.
    pause
    exit /b %NG_BUILD_RESULT%
)
start "" "%NG_ENGINE%\Binaries\Win64\UnrealEditor.exe" "%NG_PROJECT%" -ddc=InstalledNoZenLocalFallback
