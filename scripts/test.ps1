$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Force build | Out-Null
    & g++ -std=c++17 -Wall -Wextra -Wpedantic -Isrc tests/game_tests.cpp src/game/game.cpp -o build/game_tests.exe
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    & .\build\game_tests.exe
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
