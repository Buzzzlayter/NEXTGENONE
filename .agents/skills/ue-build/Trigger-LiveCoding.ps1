<#
===========================================================================
 Trigger-LiveCoding.ps1  -  fire a Live Coding compile in the open editor

 Activates the running UnrealEditor window and sends Ctrl+Alt+F11, which is
 the default Live Coding "compile and hot-patch" shortcut. After this, read
 the result as text via MCP:
   LogsToolset.GetLogEntries { category:"LogLiveCoding", pattern:"", maxEntries:30 }
   LogsToolset.GetLogEntries { category:"", pattern:"(?i)error C|error LNK", maxEntries:50 }

 BEST-EFFORT / fragile by nature: it drives the GUI via SendKeys, so it needs
 the editor window present on the same interactive desktop and briefly steals
 focus. Windows may refuse a foreground switch from a background process; if
 the keystroke doesn't land, press Ctrl+Alt+F11 in the editor manually.

 Usage:
   powershell -NoProfile -ExecutionPolicy Bypass -File Trigger-LiveCoding.ps1
   powershell -NoProfile -ExecutionPolicy Bypass -File Trigger-LiveCoding.ps1 -Diagnose

 ASCII-only on purpose (Windows PowerShell 5.1 reads BOM-less scripts as the
 system ANSI code page and corrupts non-ASCII punctuation).
===========================================================================
#>
[CmdletBinding()]
param(
    [switch]$Diagnose,                 # only report the target window, send nothing
    [int]$ActivateDelayMs = 450,
    [string]$ProjectFile,
    [switch]$BuildIfEditorClosed
)

$ErrorActionPreference = 'Stop'

if ($BuildIfEditorClosed -and -not $ProjectFile) {
    throw 'BuildIfEditorClosed requires ProjectFile.'
}

$editors = Get-Process UnrealEditor -ErrorAction SilentlyContinue |
           Where-Object { $_.MainWindowHandle -ne 0 }

if ($ProjectFile) {
    $projectPath = (Resolve-Path -LiteralPath $ProjectFile).ProviderPath
    $projectPattern = '(?:^|[\s"])' + [regex]::Escape($projectPath) + '(?=$|[\s"])'
    $matchingEditors = Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" |
                       Where-Object { $_.CommandLine -and ($_.CommandLine.Replace('/', '\') -match $projectPattern) }
    $editorIds = @($matchingEditors | Select-Object -ExpandProperty ProcessId)
    $editors = $editors | Where-Object { $_.Id -in $editorIds }

    if ($BuildIfEditorClosed -and $editorIds.Count -eq 0 -and -not $Diagnose) {
        & (Join-Path (Split-Path -Parent $projectPath) 'StartEditorFast.cmd')
        exit $LASTEXITCODE
    }
}

$proc = $editors | Select-Object -First 1

if (-not $proc) {
    Write-Output "[Trigger-LiveCoding] ERROR: no UnrealEditor window found (editor not running?)."
    Write-Output "If the project is still opening, wait for its editor window and retry."
    exit 2
}

Write-Output "[Trigger-LiveCoding] Target: PID $($proc.Id)  '$($proc.MainWindowTitle)'"

if ($Diagnose) {
    Write-Output "[Trigger-LiveCoding] Diagnose only - no keys sent."
    exit 0
}

try {
    Add-Type -AssemblyName Microsoft.VisualBasic -ErrorAction Stop
    Add-Type -AssemblyName System.Windows.Forms   -ErrorAction Stop
} catch {
    Write-Output "[Trigger-LiveCoding] ERROR: could not load WinForms/VisualBasic assemblies: $($_.Exception.Message)"
    exit 2
}

$activated = $false
try { $activated = [Microsoft.VisualBasic.Interaction]::AppActivate($proc.Id) } catch {}
if (-not $activated) {
    Write-Output "[Trigger-LiveCoding] ERROR: could not focus the editor. Press Ctrl+Alt+F11 in Unreal."
    exit 2
}
Start-Sleep -Milliseconds $ActivateDelayMs

# Ctrl+Alt+F11  ->  ^ = Ctrl, % = Alt, {F11} = F11
[System.Windows.Forms.SendKeys]::SendWait("^%{F11}")

Write-Output "[Trigger-LiveCoding] Sent Ctrl+Alt+F11. Live Coding compiles asynchronously - wait a"
Write-Output "few seconds, then read LogLiveCoding via MCP to see the result (success / errors)."
exit 0
