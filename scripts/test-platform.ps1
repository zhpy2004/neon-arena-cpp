$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Force build | Out-Null
    & g++ -std=c++17 -Wall -Wextra -Wpedantic -Isrc tests/platform_message_routing_tests.cpp src/platform/win32_app.cpp src/game/game.cpp -lgdi32 -o build/platform_message_routing_tests.exe
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    & .\build\platform_message_routing_tests.exe
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
