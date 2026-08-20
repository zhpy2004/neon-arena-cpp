param(
    [ValidateRange(100, 5000)]
    [int]$StartupWaitMilliseconds = 750
)

$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$executablePath = Join-Path $projectRoot 'build\neon_arena.exe'
$process = $null

try {
    if (-not (Test-Path -LiteralPath $executablePath -PathType Leaf)) {
        throw "Startup smoke test requires a built executable: $executablePath"
    }

    $process = Start-Process -FilePath $executablePath -WindowStyle Hidden -PassThru
    Start-Sleep -Milliseconds $StartupWaitMilliseconds
    $process.Refresh()

    if ($process.HasExited) {
        throw "Startup smoke test failed: neon_arena.exe exited early with code $($process.ExitCode)."
    }

    Write-Output "SMOKE_PASS process_id=$($process.Id) alive_after_ms=$StartupWaitMilliseconds"
} finally {
    if ($null -ne $process) {
        $process.Refresh()
        if (-not $process.HasExited) {
            Stop-Process -InputObject $process -ErrorAction Stop
            if (-not $process.WaitForExit(2000)) {
                throw "Startup smoke test cleanup failed: process $($process.Id) did not exit."
            }
        }
    }
}
