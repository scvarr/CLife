param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $repositoryRoot 'out\build\godot'

Push-Location $repositoryRoot
try {
    cmake -S . -B $buildDirectory -G 'Visual Studio 17 2022' -A x64 `
        -DCLIFE_BUILD_GODOT=ON -DCLIFE_BUILD_TESTS=OFF
    if ($LASTEXITCODE -ne 0) { throw 'Godot adapter configure failed.' }

    cmake --build $buildDirectory --config $Configuration --target clife_godot
    if ($LASTEXITCODE -ne 0) { throw 'Godot adapter build failed.' }

    Write-Output "Built apps/godot/bin/$Configuration/clife_godot.dll"
}
finally {
    Pop-Location
}
