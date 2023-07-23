param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.5'
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectFile = Join-Path $ProjectRoot 'Assignment.uproject'
$BuildTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$Editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (!(Test-Path -LiteralPath $BuildTool) -or !(Test-Path -LiteralPath $Editor)) {
    throw "Unreal Engine tools were not found under $EngineRoot"
}

& $BuildTool AssignmentEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw 'Editor build failed.' }

if (!(Test-Path -LiteralPath (Join-Path $ProjectRoot 'Content\Endless\Maps\EndlessClimb.umap'))) {
    $MapScript = Join-Path $PSScriptRoot 'CreateEndlessMap.py'
    & $Editor $ProjectFile -run=pythonscript "-script=$MapScript" '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities' -NullRHI -unattended -nosound
    if ($LASTEXITCODE -ne 0) { throw 'Boot map creation failed.' }
}

$ReportDirectory = Join-Path $ProjectRoot 'Saved\Automation'
# The Fab browser cannot restore an editor tab without an RHI; skip it only in this headless process.
& $Editor $ProjectFile -NullRHI -unattended -nop4 -nosound '-DisablePlugins=Fab' '-ExecCmds=Automation RunTests Vynix' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$ReportDirectory" -stdout -FullStdOutLogOutput
if ($LASTEXITCODE -ne 0) { throw 'Automation tests failed. See Saved/Automation and Saved/Logs.' }
Write-Host "Build and automation completed. Reports: $ReportDirectory"
