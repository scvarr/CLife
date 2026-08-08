param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$moduleRoot = Join-Path $repositoryRoot 'apps\unreal\Plugins\CLife\Source\CLifeRuntimeExternal'
$includeDestination = Join-Path $moduleRoot 'Include\clife'
$libraryDestination = Join-Path $moduleRoot 'Libraries\Win64'

Push-Location $repositoryRoot
try {
    cmake --preset vs2022
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }

    $preset = if ($Configuration -eq 'Debug') { 'vs2022-debug' } else { 'vs2022-release' }
    cmake --build --preset $preset --target clife_presets
    if ($LASTEXITCODE -ne 0) { throw 'CLife static library build failed.' }

    New-Item -ItemType Directory -Force -Path $includeDestination, $libraryDestination | Out-Null
    Copy-Item -Path (Join-Path $repositoryRoot 'include\clife\*') -Destination $includeDestination -Recurse -Force

    $buildRoot = Join-Path $repositoryRoot "out\build\vs2022\src\$Configuration"
    foreach ($library in @('clife_core.lib', 'clife_world.lib', 'clife_presets.lib')) {
        Copy-Item -LiteralPath (Join-Path $buildRoot $library) -Destination $libraryDestination -Force
    }

    Write-Output "Staged CLife $Configuration headers and libraries in $moduleRoot"
}
finally {
    Pop-Location
}
