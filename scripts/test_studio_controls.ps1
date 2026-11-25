[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Release',
    [string]$Filter = '',
    [switch]$AuditOnly,
    [string]$ReportPath = '',
    [switch]$Build
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

if ($Build -or (-not $studioExe)) {
    $cmakeExe = Find-CMake
    Write-Host "[OAE Studio Controls] Configuring OpenAudio Studio ($Config)..."
    & $cmakeExe -S $repoRoot -B $buildDir -DCMAKE_BUILD_TYPE=$Config -DBUILD_STUDIO=ON
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed with exit code $LASTEXITCODE"
    }

    Write-Host "[OAE Studio Controls] Building oae_studio ($Config)..."
    & $cmakeExe --build $buildDir --config $Config --target oae_studio
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }

    foreach ($candidate in $studioExeCandidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $studioExe = $candidate
            break
        }
    }
}

if (-not $studioExe) {
    throw "oae_studio executable not found in $buildDir."
}

$cliArgs = @('--test-ui-controls')
if (-not [string]::IsNullOrWhiteSpace($Filter)) {
    $cliArgs += "--filter=$Filter"
}
if ($AuditOnly) {
    $cliArgs += '--audit-only'
}
if (-not [string]::IsNullOrWhiteSpace($ReportPath)) {
    $cliArgs += "--report=$ReportPath"
}

Write-Host "[OAE Studio Controls] Running: `"$studioExe`" $($cliArgs -join ' ')"
$proc = Start-Process -FilePath $studioExe -ArgumentList $cliArgs -NoNewWindow -Wait -PassThru
if ($proc.ExitCode -ne 0) {
    Write-Host "RESULT: FAILED" -ForegroundColor Red
    exit 1
}

Write-Host "RESULT: OK" -ForegroundColor Green
exit 0
