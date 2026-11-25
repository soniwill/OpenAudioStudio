[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

function Find-CMake {
    $command = Get-Command cmake -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($command) { return $command.Source }

    if ($env:OS -eq 'Windows_NT') {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
            $relativePath = 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
            $paths = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.CMake.Project -find $relativePath
            if ($LASTEXITCODE -eq 0 -and $paths) { return @($paths)[0] }
        }
    }

    throw 'CMake not found. Install CMake 3.25 or newer and add it to PATH.'
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildDir = Join-Path $repoRoot "build/Studio_$Config"

Get-Process -Name "*OpenAudio Studio*", "*oae_studio*" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 200

if ($Clean -and (Test-Path -LiteralPath $buildDir)) {
    Write-Host "[OAE Studio] Cleaning $buildDir..."
    Remove-Item -LiteralPath $buildDir -Recurse -Force
}

$cmakeExe = Find-CMake

Write-Host "[OAE Studio] Configuring OpenAudio Studio ($Config)..."
& $cmakeExe -S $repoRoot -B $buildDir -DCMAKE_BUILD_TYPE=$Config -DBUILD_STUDIO=ON
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE"
}

Write-Host "[OAE Studio] Building oae_studio ($Config)..."
& $cmakeExe --build $buildDir --config $Config --target oae_studio
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

# Locate compiled executable
$studioExeCandidates = @(
    (Join-Path $buildDir "studio/oae_studio_artefacts/$Config/OpenAudio Studio.exe"),
    (Join-Path $buildDir "studio/oae_studio_artefacts/$Config/oae_studio.exe"),
    (Join-Path $buildDir "studio/$Config/oae_studio.exe"),
    (Join-Path $buildDir "studio/oae_studio.exe")
)

$studioExe = $null
foreach ($candidate in $studioExeCandidates) {
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        $studioExe = $candidate
        break
    }
}

if (-not $studioExe) {
    throw "oae_studio executable not found in expected output directories."
}

Write-Host "[OAE Studio] Executable: $studioExe"

function Invoke-StudioSmoke([string]$flag) {
    Write-Host "[OAE Studio] Running Smoke Test ($flag)..."
    $p = Start-Process -FilePath $studioExe -ArgumentList $flag -NoNewWindow -Wait -PassThru
    if ($p.ExitCode -ne 0) {
        throw "Smoke test $flag failed with exit code $($p.ExitCode)"
    }
}

Invoke-StudioSmoke '--smoke-headless'
Invoke-StudioSmoke '--smoke-project'
Invoke-StudioSmoke '--smoke-authoring'
Invoke-StudioSmoke '--smoke-timeline'
Invoke-StudioSmoke '--smoke-transport-mixer'
Invoke-StudioSmoke '--smoke-end-to-end'
Invoke-StudioSmoke '--smoke-device'
Invoke-StudioSmoke '--smoke-pro-ui'
Invoke-StudioSmoke '--smoke-mcp'
Invoke-StudioSmoke '--smoke-vu'
Invoke-StudioSmoke '--smoke-live'
Invoke-StudioSmoke '--test-ui-controls'

Write-Host "`n[OAE Studio] ALL SMOKE TESTS PASSED (Config: $Config) - RESULT: OK"

# ----------------- T16f (JP06): Portable Distribution Bundle -----------------
$bundleDir = Join-Path $repoRoot "build/Studio_Bundle_$Config"
Write-Host "`n[OAE Studio] Creating Portable Distribution Bundle in: $bundleDir"
if (Test-Path -LiteralPath $bundleDir) {
    try {
        Remove-Item -LiteralPath $bundleDir -Recurse -Force -ErrorAction Stop
    } catch {
        Get-ChildItem -LiteralPath $bundleDir -Recurse -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
    }
}
New-Item -ItemType Directory -Path $bundleDir -Force | Out-Null

# Copy application executable
Copy-Item -LiteralPath $studioExe -Destination (Join-Path $bundleDir "OpenAudio Studio.exe") -Force

# Locate and copy companion binaries (oae_audio.dll and oae_bankc.exe)
$companionCandidates = @(
    (Split-Path -Parent $studioExe),
    (Join-Path $buildDir $Config),
    $buildDir,
    (Join-Path $repoRoot "build/$Config/$Config"),
    (Join-Path $repoRoot "build/$Config")
)

$dllCopied = $false
$bankcCopied = $false

foreach ($dir in $companionCandidates) {
    if (-not $dllCopied -and (Test-Path (Join-Path $dir "oae_audio.dll"))) {
        Copy-Item (Join-Path $dir "oae_audio.dll") (Join-Path $bundleDir "oae_audio.dll") -Force
        $dllCopied = $true
    }
    if (-not $bankcCopied -and (Test-Path (Join-Path $dir "oae_bankc.exe"))) {
        Copy-Item (Join-Path $dir "oae_bankc.exe") (Join-Path $bundleDir "oae_bankc.exe") -Force
        $bankcCopied = $true
    }
}

if (-not $dllCopied) {
    throw "oae_audio.dll not found to include in portable bundle."
}
if (-not $bankcCopied) {
    throw "oae_bankc.exe not found to include in portable bundle."
}

# Copy canonical example project
$exampleSrc = Join-Path $repoRoot "example_project"
$exampleDst = Join-Path $bundleDir "example_project"
if (Test-Path -LiteralPath $exampleSrc) {
    Copy-Item -LiteralPath $exampleSrc -Destination $exampleDst -Recurse -Force
}

$manualDest = Join-Path $bundleDir 'docs/manual'
New-Item -ItemType Directory -Path $manualDest -Force | Out-Null
foreach ($entry in @('index.html', 'assets', 'chapters')) {
    Copy-Item -LiteralPath (Join-Path $repoRoot "docs/manual/$entry") -Destination $manualDest -Recurse -Force
}

# Generate README.txt
$readmeContent = @"
================================================================================
 OpenAudio Studio — Portable Desktop Distribution Bundle ($Config)
================================================================================

OpenAudio Studio is a native C++17 / JUCE 8.0.12 interactive audio authoring DAW.

DIRECTORY CONTENTS:
  - OpenAudio Studio.exe   : Standalone authoring application
  - oae_audio.dll          : OpenAudio Engine native runtime library
  - oae_bankc.exe          : Bank compiler CLI
  - example_project/       : Canonical sample project (project.json + assets + bank)
      - project.json       : Authoring definition conforming to schema v1
      - assets/            : Sample audio PCM files
      - build/             : Compiled .bank and .catalog.json files

GETTING STARTED:
  1. Double click 'OpenAudio Studio.exe' to launch the DAW.
  2. Click 'Open Project' (or press Ctrl+O) and select 'example_project/project.json'.
  3. Audition events with Play, Pause, Stop, Seek, and real-time parameter controls.
  4. Edit tracks or mixer strips, and test live in-flight preview rebuild.
  5. Click 'Build Bank' (or press Ctrl+B) to compile production banks for your game.

SHORTCUTS:
  - Ctrl+N : New Project
  - Ctrl+O : Open Project
  - Ctrl+S : Save Project
  - Ctrl+B : Build Bank
  - F1     : Help (offline manual at docs/manual/index.html)
  - Space  : Play / Pause
  - Ctrl+Z : Undo
  - Ctrl+Y : Redo

================================================================================
"@
Set-Content -Path (Join-Path $bundleDir "README.txt") -Value $readmeContent -Encoding UTF8

Write-Host "[OAE Studio] Portable Distribution Bundle successfully created:" -ForegroundColor Green
Write-Host "  -> $(Join-Path $bundleDir 'OpenAudio Studio.exe')"
Write-Host "  -> $(Join-Path $bundleDir 'oae_audio.dll')"
Write-Host "  -> $(Join-Path $bundleDir 'oae_bankc.exe')"
Write-Host "  -> $(Join-Path $bundleDir 'example_project')"
Write-Host "  -> $(Join-Path $bundleDir 'README.txt')`n"
