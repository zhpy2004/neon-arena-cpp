$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$sourceDirectory = Join-Path $projectRoot 'src'

Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Force $buildDirectory | Out-Null
    $sources = Get-ChildItem -Path $sourceDirectory -Filter '*.cpp' -File -Recurse |
        ForEach-Object { $_.FullName }
    if ($sources.Count -eq 0) {
        throw 'No C++ source files were found under src.'
    }

    $output = Join-Path $buildDirectory 'neon_arena.exe'
    & g++ -std=c++17 -Wall -Wextra -Wpedantic -Isrc @sources -mwindows -o $output -lgdi32
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
} finally {
    Pop-Location
}
