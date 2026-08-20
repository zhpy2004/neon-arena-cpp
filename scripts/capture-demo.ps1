[CmdletBinding()]
param(
    [ValidateRange(18, 180)]
    [int]$FrameCount = 54,

    [ValidateRange(40, 250)]
    [int]$FrameDelayMilliseconds = 83,

    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$gameExecutable = Join-Path $projectRoot 'build\neon_arena.exe'
if (-not (Test-Path -LiteralPath $gameExecutable)) {
    throw "Missing game executable: $gameExecutable. Run scripts/build.ps1 first."
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $projectRoot 'assets\neon-arena-demo.gif'
}

$ffmpeg = Get-Command ffmpeg -ErrorAction Stop
$outputDirectory = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class NeonDemoNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT {
        public int X;
        public int Y;
    }

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string windowName);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr window, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr window, ref POINT point);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr window);

    [DllImport("user32.dll")]
    public static extern void keybd_event(byte virtualKey, byte scanCode, uint flags, UIntPtr extraInfo);
}
'@

Add-Type -AssemblyName System.Drawing

$temporaryFrames = Join-Path $env:TEMP ("neon-arena-demo-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporaryFrames | Out-Null

$process = $null
$movementKeyIsHeld = $false

function Send-KeyTap {
    param([byte]$VirtualKey)

    [NeonDemoNative]::keybd_event($VirtualKey, 0, 0, [UIntPtr]::Zero)
    [NeonDemoNative]::keybd_event($VirtualKey, 0, 2, [UIntPtr]::Zero)
}

try {
    # A visible window is intentional here: this script records a real game session.
    $process = Start-Process -FilePath $gameExecutable -PassThru -WindowStyle Normal
    $window = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 40 -and $window -eq [IntPtr]::Zero; $attempt++) {
        Start-Sleep -Milliseconds 100
        $window = [NeonDemoNative]::FindWindow('NeonArenaWindow', 'Neon Arena')
    }
    if ($window -eq [IntPtr]::Zero) {
        throw 'Neon Arena window did not appear within 4 seconds.'
    }

    [void][NeonDemoNative]::SetForegroundWindow($window)
    Start-Sleep -Milliseconds 250

    $rectangle = New-Object NeonDemoNative+RECT
    if (-not [NeonDemoNative]::GetClientRect($window, [ref]$rectangle)) {
        throw 'Unable to read the game client bounds.'
    }

    $width = $rectangle.Right - $rectangle.Left
    $height = $rectangle.Bottom - $rectangle.Top
    if ($width -le 0 -or $height -le 0) {
        throw "Invalid game window size: ${width}x${height}"
    }

    $origin = New-Object NeonDemoNative+POINT
    $origin.X = $rectangle.Left
    $origin.Y = $rectangle.Top
    if (-not [NeonDemoNative]::ClientToScreen($window, [ref]$origin)) {
        throw 'Unable to convert client coordinates to screen coordinates.'
    }

    for ($frameIndex = 0; $frameIndex -lt $FrameCount; $frameIndex++) {
        if ($frameIndex -eq 6) {
            Send-KeyTap 0x0D # Enter: start the run.
            Start-Sleep -Milliseconds 100
            [NeonDemoNative]::keybd_event(0x44, 0, 0, [UIntPtr]::Zero) # D: move right.
            $movementKeyIsHeld = $true
        }
        if ($frameIndex -eq 28) {
            Send-KeyTap 0x20 # Space: dash.
        }
        if ($frameIndex -eq 42 -and $movementKeyIsHeld) {
            [NeonDemoNative]::keybd_event(0x44, 0, 2, [UIntPtr]::Zero)
            $movementKeyIsHeld = $false
        }

        $bitmap = New-Object System.Drawing.Bitmap $width, $height
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.CopyFromScreen($origin.X, $origin.Y, 0, 0, $bitmap.Size)
            $framePath = Join-Path $temporaryFrames ('frame_{0:D3}.png' -f $frameIndex)
            $bitmap.Save($framePath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $graphics.Dispose()
            $bitmap.Dispose()
        }

        Start-Sleep -Milliseconds $FrameDelayMilliseconds
    }

    $framesPerSecond = [Math]::Round(1000.0 / $FrameDelayMilliseconds, 2)
    $filters = 'fps=' + $framesPerSecond + ',scale=960:-1:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse'
    & $ffmpeg.Source -y -framerate $framesPerSecond -start_number 0 -i (Join-Path $temporaryFrames 'frame_%03d.png') -filter_complex $filters -loop 0 $OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "FFmpeg GIF export failed with exit code: $LASTEXITCODE"
    }

    Get-Item -LiteralPath $OutputPath | Select-Object FullName, Length, LastWriteTime
}
finally {
    if ($movementKeyIsHeld) {
        [NeonDemoNative]::keybd_event(0x44, 0, 2, [UIntPtr]::Zero)
    }
    if ($null -ne $process) {
        $process.Refresh()
        if (-not $process.HasExited) {
            [void]$process.CloseMainWindow()
            if (-not $process.WaitForExit(1000)) {
                Stop-Process -Id $process.Id -ErrorAction Stop
                [void]$process.WaitForExit(1000)
            }
        }
    }
    if (Test-Path -LiteralPath $temporaryFrames) {
        Remove-Item -LiteralPath $temporaryFrames -Recurse -Force
    }
}
