param(
    [Parameter(Mandatory=$true)][string]$GameMode,
    [int]$LagMs=0, [int]$LossPercent=0, [int]$Port=17777
)
$ErrorActionPreference='Stop'
if ($GameMode -notmatch '^/(Game|Script)/[A-Za-z0-9_./]+$') { throw 'GameMode must be a configured test GameMode class path' }
$ngRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$ngProject = Join-Path $ngRoot 'NEXTGENONE.uproject'
$ngAssoc = (Get-Content -Raw $ngProject | ConvertFrom-Json).EngineAssociation
$ngEngine = (Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$ngAssoc" -ErrorAction SilentlyContinue).InstalledDirectory
if (!$ngEngine) { throw "Engine $ngAssoc not found in registry" }
$ngExe = Join-Path $ngEngine 'Engine/Binaries/Win64/UnrealEditor.exe'
$ngRun = "NGCombat_${LagMs}ms_${LossPercent}loss"
$ngStamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$ngLogs = Join-Path $ngRoot "Saved/Logs/${ngRun}_$ngStamp"
New-Item -ItemType Directory -Force -Path $ngLogs | Out-Null
$ngProcs = @()
$ngQuotedProject = '"{0}"' -f $ngProject
$ngCommon = @('-unattended','-nullrhi','-nosound','-NoLiveCoding','-ddc=InstalledNoZenLocalFallback','-NGCombatSmoke','-nosplash','-NoLoadingScreen',"PktLag=$LagMs","PktLoss=$LossPercent")
try {
    $ngServerLog = Join-Path $ngLogs 'Server.log'
    $ngLogArg = '-abslog="{0}"' -f $ngServerLog
    $ngProcs += Start-Process -FilePath $ngExe -WorkingDirectory $ngRoot -WindowStyle Hidden -PassThru -ArgumentList (@($ngQuotedProject,"/Game/Maps/Gameplay?game=$GameMode",'-server',"-port=$Port",$ngLogArg) + $ngCommon)
    $ngReadyDeadline = (Get-Date).AddSeconds(60)
    $ngListening = $false
    while ((Get-Date) -lt $ngReadyDeadline) {
        if ((Test-Path $ngServerLog) -and (Select-String -Quiet -LiteralPath $ngServerLog -Pattern 'GameNetDriver.*listening on port')) { $ngListening = $true; break }
        if ($ngProcs[0].HasExited) { throw 'Server exited during startup' }
        Start-Sleep -Milliseconds 500
    }
    if (!$ngListening) { throw 'Server did not start listening before startup timeout' }
    for ($ngClient=1; $ngClient -le 2; $ngClient++) {
        $ngClientLog = Join-Path $ngLogs "Client$ngClient.log"
        $ngLogArg = '-abslog="{0}"' -f $ngClientLog
        $ngProcs += Start-Process -FilePath $ngExe -WorkingDirectory $ngRoot -WindowStyle Hidden -PassThru -ArgumentList (@($ngQuotedProject,"127.0.0.1:$Port",'-game',$ngLogArg) + $ngCommon)
    }
    $ngDeadline = (Get-Date).AddSeconds(150)
    while ((Get-Date) -lt $ngDeadline) {
        if (Select-String -Quiet -LiteralPath $ngServerLog -Pattern 'NG_SMOKE_FAIL') { throw "Smoke failed; see $ngServerLog" }
        if (Select-String -Quiet -LiteralPath $ngServerLog -Pattern 'NG_SMOKE_PASS') {
            for ($ngClient=1; $ngClient -le 2; $ngClient++) {
                $ngClientLog = Join-Path $ngLogs "Client$ngClient.log"
                if (!(Select-String -Quiet -LiteralPath $ngClientLog -Pattern 'NG_COSMETIC')) { throw "Client $ngClient did not receive shot cosmetics" }
            }
            Select-String -LiteralPath $ngServerLog -Pattern 'NG_ASC_READY|NG_SHOT|NG_DEATH|NG_CLIENT_REPORT|NG_SMOKE_PASS' | ForEach-Object { $_.Line }
            Write-Output "PASS: $ngRun; logs: $ngLogs"
            exit 0
        }
        if (@($ngProcs | Where-Object HasExited).Count) { throw 'A test process exited unexpectedly' }
        Start-Sleep -Milliseconds 500
    }
    throw "Smoke timed out; see $ngServerLog"
} finally {
    # Only the exact processes launched by this invocation; never the user's editor.
    foreach ($ngProc in $ngProcs) { if (!$ngProc.HasExited) { Stop-Process -Id $ngProc.Id -ErrorAction SilentlyContinue } }
}
