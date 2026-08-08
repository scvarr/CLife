param(
    [Parameter(Mandatory = $true)]
    [string]$UnrealEngineRoot
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $repositoryRoot 'apps\unreal\CLifeDemo.uproject'
$buildScript = Join-Path $UnrealEngineRoot 'Engine\Build\BatchFiles\Build.bat'

if (-not (Test-Path -LiteralPath $buildScript)) {
    throw "Unreal Build.bat was not found under '$UnrealEngineRoot'."
}

& (Join-Path $PSScriptRoot 'build_unreal_clife.ps1') -Configuration Release
if ($LASTEXITCODE -ne 0) { throw 'CLife library staging failed.' }

& $buildScript UnrealEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw 'Unreal Development Editor build failed.' }
