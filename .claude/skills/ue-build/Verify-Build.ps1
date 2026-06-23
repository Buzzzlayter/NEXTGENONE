<#
===========================================================================
 Verify-Build.ps1  -  agent-facing C++ build verification

 Compiles a target with UnrealBuildTool and prints a COMPACT, plain-text
 summary (PASS/FAIL + the actual compiler errors/warnings) to stdout, so an
 agent writing C++ can see real compiler output instead of guessing the API.

 Self-contained: finds the .uproject beside this script, derives the target
 from it, and resolves the engine from EngineAssociation via the registry
 (same logic as Build.bat). Nothing is hard-coded.

 Usage (from the project root):
   powershell -NoProfile -ExecutionPolicy Bypass -File Verify-Build.ps1
   powershell -NoProfile -ExecutionPolicy Bypass -File Verify-Build.ps1 -Game
   powershell -NoProfile -ExecutionPolicy Bypass -File Verify-Build.ps1 -Config DebugGame

 The full UBT log is written to Saved\Logs\AgentBuild.log for deep dives.
 Exit code: 0 on success, non-zero on failure (CI / agent friendly).

 NOTE on the editor being open: UBT will still COMPILE every .cpp (so all
 C++ errors/warnings are reported), but the final LINK step can't overwrite
 a DLL the running editor has loaded. This script detects that case and
 reports it as "LINK BLOCKED (editor open)" rather than a code error - the
 compiler diagnostics above it are still valid. Close the editor for a full
 build, or use Live Coding (Ctrl+Alt+F11) to hot-patch.

 ASCII-only on purpose: Windows PowerShell 5.1 reads BOM-less scripts as the
 system ANSI code page, which corrupts non-ASCII punctuation. Keep it ASCII.
===========================================================================
#>
[CmdletBinding()]
param(
    [switch]$Game,                       # build the game target instead of the editor target
    [string]$Platform = "Win64",
    [string]$Config   = "Development",   # Debug | DebugGame | Development | Shipping
    [int]$MaxLines    = 80               # cap on error/warning lines echoed to stdout
)

$ErrorActionPreference = 'Stop'

function Fail([string]$msg) { Write-Output "[Verify-Build] ERROR: $msg"; exit 2 }

# --- 1) locate the .uproject (walk up from this script) and derive target ---
# The script may live in .claude/skills/ue-build/, so search ancestors for the
# nearest folder that holds a .uproject; that folder is the project root.
$uproject = $null; $root = $null; $dir = $PSScriptRoot
while ($dir) {
    $cand = Get-ChildItem -Path $dir -Filter *.uproject -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cand) { $uproject = $cand; $root = $dir; break }
    $parent = Split-Path $dir -Parent
    if (-not $parent -or $parent -eq $dir) { break }
    $dir = $parent
}
if (-not $uproject) { Fail "No .uproject found in any folder above $PSScriptRoot." }
$projName = [IO.Path]::GetFileNameWithoutExtension($uproject.Name)
$target   = if ($Game) { $projName } else { "${projName}Editor" }

# --- 2) read EngineAssociation from the .uproject --------------------------
try { $assoc = (Get-Content $uproject.FullName -Raw | ConvertFrom-Json).EngineAssociation }
catch { Fail "Could not read EngineAssociation from $($uproject.Name)." }
if ([string]::IsNullOrWhiteSpace($assoc)) { Fail "EngineAssociation is empty (source build beside the project?)." }

# --- 3) resolve the engine install dir from the registry -------------------
function Resolve-EngineRoot([string]$a) {
    if ($a -match '^\{.*\}$') {
        foreach ($hive in 'HKCU:', 'HKLM:') {
            $key = "$hive\SOFTWARE\Epic Games\Unreal Engine\Builds"
            try { $v = (Get-ItemProperty -Path $key -Name $a -ErrorAction Stop).$a; if ($v) { return $v } } catch {}
        }
    } else {
        foreach ($key in @("HKLM:\SOFTWARE\EpicGames\Unreal Engine\$a",
                           "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$a")) {
            try { $v = (Get-ItemProperty -Path $key -Name 'InstalledDirectory' -ErrorAction Stop).InstalledDirectory; if ($v) { return $v } } catch {}
        }
    }
    return $null
}
$engineRoot = Resolve-EngineRoot $assoc
if (-not $engineRoot) { Fail "Could not resolve an engine for EngineAssociation `"$assoc`"." }
$buildBat = Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat'
if (-not (Test-Path $buildBat)) { Fail "Engine Build.bat not found at `"$buildBat`"." }

# --- 4) is the editor running? (affects link, not compile) -----------------
$editorUp = [bool](Get-Process UnrealEditor -ErrorAction SilentlyContinue)

# --- 5) run UBT, capturing combined stdout/stderr --------------------------
$logDir = Join-Path $root 'Saved\Logs'
if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Force -Path $logDir | Out-Null }
$logPath = Join-Path $logDir 'AgentBuild.log'

$cmdLine = "`"$buildBat`" $target $Platform $Config -project=`"$($uproject.FullName)`" -waitmutex"
$started = Get-Date
$lines   = & cmd /c "$cmdLine 2>&1"
$exit    = $LASTEXITCODE
$elapsed = [math]::Round(((Get-Date) - $started).TotalSeconds, 1)
$lines | Out-File -FilePath $logPath -Encoding utf8

# --- 6) parse diagnostics --------------------------------------------------
# MSVC: "Foo.cpp(42): error C2065: ..."  /  link: "LINK : fatal error LNK1168 ..."
# UBT : "ERROR: ..."                      /  warnings: "... : warning C4101: ..."
$errRx  = '(?i)(error\s+(C|LNK|D|MSB|RC)\d|^\s*ERROR:|:\s*error:)'
$warnRx = '(?i)(warning\s+(C|LNK|D|MSB)\d|:\s*warning:|^\s*WARNING:)'
$lockRx = '(?i)(LNK1168|cannot open .*\.(dll|exe|lib) for writing|being used by another process)'
$liveRx = '(?i)Unable to build while Live Coding is active'

$errors   = @($lines | Where-Object { $_ -match $errRx }  | Select-Object -Unique)
$warnings = @($lines | Where-Object { $_ -match $warnRx } | Select-Object -Unique)
$linkLock = @($lines | Where-Object { $_ -match $lockRx })
$liveBlock = @($lines | Where-Object { $_ -match $liveRx }).Count -gt 0

# A link-lock against a loaded DLL is expected when the editor is open: don't
# count those lines as genuine compiler errors.
$realErrors = @($errors | Where-Object { $_ -notmatch $lockRx })

# --- 7) compact, agent-readable report -------------------------------------
if ($exit -eq 0) {
    $status = "PASS"
} elseif ($liveBlock) {
    $status = "BLOCKED (Live Coding active)"
} elseif ($realErrors.Count -eq 0 -and $linkLock.Count -gt 0) {
    $status = "COMPILE-OK / LINK-BLOCKED"
} else {
    $status = "FAIL"
}
if ($editorUp) { $editorNote = "YES (full relink will be blocked)" } else { $editorNote = "no" }

Write-Output "=== Verify-Build: $target $Platform $Config ==="
Write-Output "Engine        : $engineRoot"
Write-Output "Editor running: $editorNote"
Write-Output "UBT exit code : $exit   (${elapsed}s)"
Write-Output "RESULT        : $status"
Write-Output ""

if ($realErrors.Count -gt 0) {
    Write-Output "Errors ($($realErrors.Count)):"
    $realErrors | Select-Object -First $MaxLines | ForEach-Object { Write-Output "  $_" }
    if ($realErrors.Count -gt $MaxLines) {
        $more = $realErrors.Count - $MaxLines
        Write-Output "  ... (+$more more, see $logPath)"
    }
    Write-Output ""
}

if ($warnings.Count -gt 0) {
    Write-Output "Warnings ($($warnings.Count)):"
    $warnings | Select-Object -First $MaxLines | ForEach-Object { Write-Output "  $_" }
    if ($warnings.Count -gt $MaxLines) {
        $more = $warnings.Count - $MaxLines
        Write-Output "  ... (+$more more, see $logPath)"
    }
    Write-Output ""
}

if ($liveBlock) {
    Write-Output "BLOCKED: Live Coding is active in the running editor, so UBT refused to build"
    Write-Output "(nothing was compiled). To verify C++ from here you have two options:"
    Write-Output "  1) Close the editor, then re-run this script for a full offline build, OR"
    Write-Output "  2) Keep the editor open and trigger a Live Coding compile (Ctrl+Alt+F11);"
    Write-Output "     read its result from the output log, category LogLiveCoding."
    Write-Output ""
}

if ($linkLock.Count -gt 0 -and $realErrors.Count -eq 0 -and -not $liveBlock) {
    Write-Output "LINK BLOCKED: the running editor holds the module DLL open - compile succeeded,"
    Write-Output "but the new DLL could not be written. Close the editor for a full build, or use"
    Write-Output "Live Coding (Ctrl+Alt+F11) to hot-patch the running editor."
    Write-Output ""
}

if ($realErrors.Count -eq 0 -and $warnings.Count -eq 0 -and $exit -eq 0) {
    Write-Output "No compiler diagnostics - target is up to date / built clean."
    Write-Output ""
}

Write-Output "Full log: $logPath"

# Distinct exit codes so the agent can branch on them:
#   0 = PASS (or link-only lock, which still means the code compiled)
#   3 = BLOCKED, nothing compiled (Live Coding active / editor open) - not a code error
#   other = real build failure (see Errors above)
if ($liveBlock) { exit 3 }
if ($exit -ne 0 -and $realErrors.Count -eq 0 -and $linkLock.Count -gt 0) { exit 0 }
exit $exit
