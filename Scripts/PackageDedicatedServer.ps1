param(
    [Parameter(Mandatory=$true)][string]$EngineRoot,
    [ValidateSet('Win64','Linux')][string]$Platform='Win64',
    [string]$OutputDirectory
)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (Test-Path -LiteralPath (Join-Path $EngineRoot 'Engine/Build/InstalledBuild.txt')) {
    throw 'This engine is an installed build. Use a source build of UE 5.8 with server support for NEXTGENONEServer. No build/cook was started.'
}
$uat=Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat'
if (!(Test-Path -LiteralPath $uat)) { throw "RunUAT missing: $uat" }
if (!$OutputDirectory) { $OutputDirectory=Join-Path $projectRoot "Saved/ServerPackages/$Platform" }
$projectFile=Join-Path $projectRoot 'NEXTGENONE.uproject'
$arguments=@('BuildCookRun',"-project=$projectFile",'-noP4','-server','-noclient','-build','-cook','-stage','-pak','-archive',"-serverplatform=$Platform",'-serverconfig=Development','-map=/Game/Maps/Gameplay',"-archivedirectory=$OutputDirectory",'-utf8output')
& $uat @arguments
if ($LASTEXITCODE -ne 0) { throw "Server packaging failed with exit code $LASTEXITCODE" }
