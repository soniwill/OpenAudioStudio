[CmdletBinding()]
param(
    [Parameter()]
    [ValidateSet('docs', 'native', 'studio', 'unity')]
    [string[]]$Layer,
    [switch]$All,
    [switch]$SkipUnityIfUnavailable
)

$ErrorActionPreference = 'Stop'

function Invoke-Checked {
    param([string]$Executable, [string[]]$Arguments)
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable exited with code $LASTEXITCODE."
    }
}

function Get-ChangedPaths {
    $diff = & git -C $repoRoot -c core.quotePath=false diff --name-only --no-renames HEAD --
    if ($LASTEXITCODE -ne 0) { throw 'git diff failed.' }
    $untracked = & git -C $repoRoot -c core.quotePath=false ls-files --others --exclude-standard
    if ($LASTEXITCODE -ne 0) { throw 'git ls-files failed.' }
    return @(@($diff) + @($untracked) | Where-Object { $_ } | Sort-Object -Unique)
}

function Get-DetectedLayers {
    param([string[]]$Paths)
    $detected = [System.Collections.Generic.List[string]]::new()
    foreach ($path in $Paths) {
        if ($path -match '^(native/|CMakeLists\.txt$|scripts/build\.ps1$)') { $detected.Add('native') }
        if ($path -match '^(studio/|scripts/build_studio\.ps1$)') { $detected.Add('studio') }
        if ($path -match '^(unity/|scripts/unity_test\.ps1$|scripts/deploy_unity\.ps1$)') { $detected.Add('unity') }
        if ($path -match '^(docs/|openspec/|AGENTS\.md$|README\.md$|\.gitignore$|\.gitmodules$|scripts/verify\.ps1$)') { $detected.Add('docs') }
    }
    return @($detected | Sort-Object -Unique)
}

function Test-LocalMarkdownLinks {
    param([string]$FilePath)
    if (-not (Test-Path -LiteralPath $FilePath -PathType Leaf)) {
        throw "Required documentation file is missing: $FilePath"
    }
    $content = Get-Content -LiteralPath $FilePath -Raw -Encoding UTF8
    $linkPattern = '\[[^\]]*\]\(\s*(?:<(?<target>[^>]+)>|(?<target>[^\s)]+))(?:\s+["''][^\r\n]*?["''])?\s*\)'
    foreach ($match in [regex]::Matches($content, $linkPattern)) {
        $target = $match.Groups['target'].Value.Trim()
        if (-not $target -or $target.StartsWith('#') -or $target -match '^[a-zA-Z][a-zA-Z0-9+.-]*:') { continue }
        $target = ($target -split '[?#]', 2)[0]
        if (-not $target) { continue }
        $decoded = [Uri]::UnescapeDataString($target)
        $resolved = [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $FilePath) $decoded))
        if (-not (Test-Path -LiteralPath $resolved)) {
            throw "Broken relative link in '$((Resolve-Path -Relative $FilePath))': $target"
        }
    }
}

function Invoke-DocsGate {
    Write-Host '[docs] Validating all OpenSpec artifacts (strict)...'
    $openSpec = Get-Command openspec -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $openSpec) { throw 'openspec CLI is unavailable.' }
    Invoke-Checked $openSpec.Source @('validate', '--all', '--strict')

    $memoryPath = Join-Path $repoRoot 'docs/ai/memory.md'
    if (-not (Test-Path -LiteralPath $memoryPath -PathType Leaf)) { throw 'docs/ai/memory.md is missing.' }
    $lineCount = @(Get-Content -LiteralPath $memoryPath).Count
    if ($lineCount -gt 100) { throw "docs/ai/memory.md has $lineCount lines; maximum is 100." }
    Write-Host "[docs] docs/ai/memory.md: $lineCount/100 lines."

    foreach ($relative in @('README.md', 'AGENTS.md', 'docs/ai/rules.md', 'docs/specs/README.md', 'docs/legacy/README.md')) {
        Test-LocalMarkdownLinks (Join-Path $repoRoot $relative)
        Write-Host "[docs] Relative links OK: $relative"
    }
}

function Test-UnityResults {
    param([datetime]$StartedAt)

    foreach ($platform in @('EditMode', 'PlayMode')) {
        $path = Join-Path $repoRoot "build/UnityTestResults/results_$platform.xml"
        $report = Get-Item -LiteralPath $path -ErrorAction SilentlyContinue
        if (-not $report -or $report.LastWriteTimeUtc -lt $StartedAt) {
            throw "Unity did not produce a current $platform report: $path"
        }
        [xml]$results = Get-Content -LiteralPath $path -Raw
        $run = $results.DocumentElement
        if ($run.Name -ne 'test-run' -or $run.result -ne 'Passed' -or
            [int]$run.failed -ne 0 -or [int]$run.passed -lt 1) {
            throw "Unity $platform did not pass. See $path"
        }
    }
}

function Test-LoopbackAvailable {
    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
    try {
        $listener.Start()
        return $true
    }
    catch [Net.Sockets.SocketException] {
        if ($_.Exception.SocketErrorCode -eq [Net.Sockets.SocketError]::AccessDenied) {
            return $false
        }
        throw
    }
    finally {
        $listener.Stop()
    }
}

try {
    $repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).ProviderPath
    if ($All -and $PSBoundParameters.ContainsKey('Layer')) { throw 'Use either -All or -Layer, not both.' }
    if ($SkipUnityIfUnavailable -and -not $All) { throw '-SkipUnityIfUnavailable is only valid with -All.' }

    if ($All) {
        $layers = @('docs', 'native', 'studio', 'unity')
    } elseif ($PSBoundParameters.ContainsKey('Layer')) {
        $layers = @($Layer | Sort-Object -Unique)
    } else {
        $layers = @(Get-DetectedLayers (Get-ChangedPaths))
        if ($layers.Count -eq 0) {
            Write-Host 'No changed paths map to a verification layer.'
            Write-Output 'RESULT: OK'
            exit 0
        }
        Write-Host "Detected layers: $($layers -join ', ')"
    }

    $pending = [System.Collections.Generic.List[string]]::new()
    foreach ($currentLayer in $layers) {
        Write-Host "=== Verifying $currentLayer ==="
        switch ($currentLayer) {
            'docs' { Invoke-DocsGate }
            'native' {
                & (Join-Path $PSScriptRoot 'build.ps1') -Config Debug
                if ($LASTEXITCODE -ne 0) { throw "Native Debug gate failed with exit code $LASTEXITCODE." }
                & (Join-Path $PSScriptRoot 'build.ps1') -Config Release
                if ($LASTEXITCODE -ne 0) { throw "Native Release gate failed with exit code $LASTEXITCODE." }
            }
            'studio' {
                $juceCmake = Join-Path $repoRoot 'studio/juce/CMakeLists.txt'
                $juceStatus = & git -C $repoRoot submodule status -- studio/juce
                if ($LASTEXITCODE -ne 0 -or $juceStatus -notmatch '^ [0-9a-f]+ studio/juce(?: |$)' -or
                    -not (Test-Path -LiteralPath $juceCmake -PathType Leaf)) {
                    throw 'JUCE submodule is missing or differs from the indexed revision. Run: git submodule update --init --recursive'
                }
                if ($All -and -not (Test-LoopbackAvailable)) {
                    $pending.Add('studio: access to TCP loopback denied')
                    break
                }
                & (Join-Path $PSScriptRoot 'build_studio.ps1') -Config Release
                if ($LASTEXITCODE -ne 0) { throw "Studio Release gate failed with exit code $LASTEXITCODE." }
            }
            'unity' {
                $dotnet = Get-Command dotnet -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
                $unityExe = Get-ChildItem 'C:\Program Files\Unity\Hub\Editor\*\Editor\Unity.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
                if (-not $dotnet -or -not $unityExe) {
                    if ($All -and $SkipUnityIfUnavailable) {
                        Write-Host '[unity] SKIPPED: required .NET or Unity Editor unavailable.'
                        break
                    }
                    if ($All) {
                        $pending.Add('unity: required .NET SDK or Unity Editor unavailable')
                        break
                    }
                    throw 'Unity gate unavailable: .NET SDK and Unity Editor are required.'
                }
                $startedAt = [DateTime]::UtcNow
                & (Join-Path $PSScriptRoot 'unity_test.ps1') -UnityPath $unityExe.FullName -TestPlatform Both
                if ($LASTEXITCODE -ne 0) { throw "Unity test gate failed with exit code $LASTEXITCODE." }
                Test-UnityResults $startedAt
            }
        }
    }

    if ($pending.Count -gt 0) {
        foreach ($item in $pending) { Write-Host "PENDING/BLOCKED: $item" }
        Write-Output 'RESULT: PENDING/BLOCKED'
        exit 2
    }
    Write-Output 'RESULT: OK'
    exit 0
}
catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    Write-Output 'RESULT: FAILED'
    exit 1
}
