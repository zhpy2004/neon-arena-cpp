$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$testScripts = @(
    (Join-Path $projectRoot 'scripts\test.ps1'),
    (Join-Path $projectRoot 'scripts\test-platform.ps1')
)

Push-Location $projectRoot
try {
    foreach ($testScript in $testScripts) {
        if (-not (Test-Path -LiteralPath $testScript -PathType Leaf)) {
            throw "Required test script was not found: $testScript"
        }

        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $testScript
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
} finally {
    Pop-Location
}
