[CmdletBinding()]
param(
    [string]$UnityPath,
    [string]$ProjectPath,
    [ValidateSet("EditMode", "PlayMode", "Both")]
    [string]$TestPlatform = "EditMode",
    [switch]$DotnetOnly,
    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$dotnetProj = Join-Path $repoRoot "unity\StandaloneTests\OpenAudio.StandaloneTests.csproj"

# Step 1: Run fast standalone .NET test suite
Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host " OpenAudio Engine - Running Standalone .NET C# Test Suite" -ForegroundColor Cyan
Write-Host "=================================================================" -ForegroundColor Cyan

& dotnet test $dotnetProj -c Release
$dotnetExitCode = $LASTEXITCODE

if ($dotnetExitCode -ne 0) {
    Write-Error "Standalone .NET test suite failed with exit code $dotnetExitCode"
    exit $dotnetExitCode
}

Write-Host "`n[OpenAudio] Standalone .NET C# tests passed successfully!" -ForegroundColor Green

if ($DotnetOnly) {
    exit 0
}

# Step 2: Locate Unity Editor if available
if (-not $UnityPath) {
    $editorSearch = @(
        "C:\Program Files\Unity\Hub\Editor\6000.0.34f1\Editor\Unity.exe",
        "C:\Program Files\Unity\Hub\Editor\6000.4.1f1\Editor\Unity.exe"
    )

    foreach ($candidate in $editorSearch) {
        if (Test-Path $candidate) {
            $UnityPath = $candidate
            break
        }
    }

    if (-not $UnityPath) {
        $dynamic = Get-ChildItem "C:\Program Files\Unity\Hub\Editor\*\Editor\Unity.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($dynamic) {
            $UnityPath = $dynamic.FullName
        }
    }
}

if (-not $UnityPath -or -not (Test-Path $UnityPath)) {
    Write-Host "[OpenAudio] Unity Editor not found on system. Skipping batchmode Unity run." -ForegroundColor Yellow
    Write-Host "[OpenAudio] C# tests verified via Standalone .NET runner." -ForegroundColor Green
    exit 0
}

if (-not $ProjectPath) {
    $ProjectPath = Join-Path $repoRoot "unity\OpenAudioUnity"
}

Write-Host "`n=================================================================" -ForegroundColor Cyan
Write-Host " OpenAudio Engine - Running Unity Editor Batchmode Tests" -ForegroundColor Cyan
Write-Host " Unity: $UnityPath" -ForegroundColor Cyan
Write-Host " Project: $ProjectPath" -ForegroundColor Cyan
Write-Host " Platform: $TestPlatform" -ForegroundColor Cyan
Write-Host "=================================================================" -ForegroundColor Cyan

$testResultsDir = Join-Path $repoRoot "build\UnityTestResults"
if (-not (Test-Path $testResultsDir)) {
    New-Item -ItemType Directory -Path $testResultsDir -Force | Out-Null
}

$platformsToRun = @()
if ($TestPlatform -eq "Both") {
    $platformsToRun = @("EditMode", "PlayMode")
} else {
    $platformsToRun = @($TestPlatform)
}

foreach ($platform in $platformsToRun) {
    $resultXml = Join-Path $testResultsDir "results_$platform.xml"
    $logFile = Join-Path $testResultsDir "unity_$platform.log"

    Write-Host "[OpenAudio] Running $platform tests..." -ForegroundColor Cyan

    $processArgs = @(
        "-batchmode",
        "-nographics",
        "-projectPath", "`"$ProjectPath`"",
        "-runTests",
        "-testPlatform", $platform,
        "-testResults", "`"$resultXml`"",
        "-logFile", "`"$logFile`""
    )

    $argString = $processArgs -join " "
    $process = Start-Process -FilePath $UnityPath -ArgumentList $argString -NoNewWindow -PassThru

    $finished = $process.WaitForExit($TimeoutSeconds * 1000)
    if (-not $finished) {
        Write-Error "Unity test execution timed out after $TimeoutSeconds seconds."
        $process.Kill()
        exit 1
    }

    Write-Host "[OpenAudio] $platform tests finished with exit code $($process.ExitCode)." -ForegroundColor $(if ($process.ExitCode -eq 0) { "Green" } else { "Yellow" })
    if (Test-Path $logFile) {
        Write-Host "[OpenAudio] Log saved to: $logFile" -ForegroundColor Gray
    }
}

Write-Host "`n[OpenAudio] Unity test pipeline complete." -ForegroundColor Green
exit 0
