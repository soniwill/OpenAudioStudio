param(
    [string]$OutDir = "docs/manual/assets/img",
    [string]$ProjectPath = "example_project/project.json",
    [string]$FileName = "studio-overview.png",
    [int]$Width = 1440,
    [int]$Height = 900,
    [int]$WaitMs = 2500,
    [switch]$KeepOpen
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;

public class WinCapture {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdcBlt, uint nFlags);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
}
"@

$repoRoot = (Resolve-Path ".").Path
$exePath = Join-Path $repoRoot "build\Studio_Bundle_Release\OpenAudio Studio.exe"
$projAbs = Join-Path $repoRoot $ProjectPath
$outAbsDir = Join-Path $repoRoot $OutDir
New-Item -ItemType Directory -Force -Path $outAbsDir | Out-Null

$existing = Get-Process -Name "OpenAudio Studio" -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
$startedByUs = $false

if (-not $existing) {
    $proc = Start-Process -FilePath $exePath -ArgumentList "`"$projAbs`"", "--mcp-port=7451" -PassThru
    $startedByUs = $true
    $deadline = (Get-Date).AddSeconds(15)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 300
        $proc.Refresh()
        if ($proc.MainWindowHandle -ne 0) { break }
    }
    $hwnd = $proc.MainWindowHandle
} else {
    $hwnd = $existing.MainWindowHandle
}

if ($hwnd -eq 0) {
    throw "Failed to obtain OpenAudio Studio window handle."
}

[WinCapture]::MoveWindow($hwnd, 40, 40, $Width, $Height, $true) | Out-Null
[WinCapture]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds $WaitMs

$rect = New-Object WinCapture+RECT
[WinCapture]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
$w = $rect.Right - $rect.Left
$h = $rect.Bottom - $rect.Top

$bmp = New-Object System.Drawing.Bitmap($w, $h)
$gfx = [System.Drawing.Graphics]::FromImage($bmp)
$gfx.CopyFromScreen($rect.Left, $rect.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))

$outFile = Join-Path $outAbsDir $FileName
$bmp.Save($outFile, [System.Drawing.Imaging.ImageFormat]::Png)
$gfx.Dispose()
$bmp.Dispose()

Write-Output "CAPTURED:$outFile (${w}x${h})"

if ($startedByUs -and (-not $KeepOpen)) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}
