param(
    [ValidateRange(1,65535)][int]$Port=7777,
    [switch]$EditorServer,
    [string]$EngineRoot,
    [string]$ServerExecutable
)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$projectFile=Join-Path $projectRoot 'NEXTGENONE.uproject'
$logFile=Join-Path $projectRoot 'Saved/Logs/DedicatedServer.log'
New-Item -ItemType Directory -Path (Split-Path $logFile) -Force | Out-Null
if ($EditorServer) {
    if (!$EngineRoot) {
        $association=(Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json).EngineAssociation
        $EngineRoot=(Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$association" -ErrorAction SilentlyContinue).InstalledDirectory
        if (!$EngineRoot) { $EngineRoot=(Get-ItemProperty "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$association" -ErrorAction SilentlyContinue).InstalledDirectory }
        if (!$EngineRoot) { $EngineRoot=(Get-ItemProperty 'HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds' -ErrorAction SilentlyContinue).$association }
    }
    if (!$EngineRoot) { throw 'Pass -EngineRoot for the UE engine installation.' }
    $serverBinary=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    $arguments=@('"'+$projectFile+'"','/Game/Maps/Gameplay','-server','-unattended','-NullRHI','-nosound',"-port=$Port",'-abslog="'+$logFile+'"','-NoLiveCoding')
} else {
    $serverBinary=$ServerExecutable
    if (!$serverBinary) { $serverBinary=Join-Path $projectRoot 'Binaries/Win64/NEXTGENONEServer.exe' }
    $arguments=@('/Game/Maps/Gameplay','-unattended','-nosound',"-port=$Port",'-abslog="'+$logFile+'"')
}
if (!(Test-Path -LiteralPath $serverBinary)) { throw "Server binary missing: $serverBinary. Build and stage a server package, or explicitly use -EditorServer for local development." }
$process=Start-Process -FilePath $serverBinary -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
Write-Output "Dedicated server PID=$($process.Id), UDP=$Port, log=$logFile"
