[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug',
    [switch]$NoTests,
    [switch]$Clean,
    [ValidateNotNullOrEmpty()]
    [string]$TestFilter,
    [switch]$Full
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

function Invoke-Checked {
    param([string]$Executable, [string[]]$Arguments)
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "${Executable} exited with code $LASTEXITCODE."
    }
}

try {
    if (($Full -or $NoTests) -and $PSBoundParameters.ContainsKey('TestFilter')) {
        throw '-TestFilter requires an incremental build with tests enabled.'
    }
    if ($Full -and $NoTests) { throw '-Full requires tests.' }

    $root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).ProviderPath
    $buildRoot = Join-Path $root 'build'
    $buildDir = Join-Path $buildRoot $Config
    foreach ($path in @($buildRoot, $buildDir)) {
        $item = Get-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
        if ($item -and (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint))) {
            throw "Expected an ordinary build directory at $path."
        }
    }

    $cmake = Find-CMake
    $ctest = Join-Path (Split-Path -Parent $cmake) $(if ($env:OS -eq 'Windows_NT') { 'ctest.exe' } else { 'ctest' })
    if (-not $NoTests -and -not (Test-Path -LiteralPath $ctest -PathType Leaf)) {
        throw "CTest not found alongside CMake at $ctest."
    }
    if (($Clean -or $Full) -and (Test-Path -LiteralPath $buildDir)) {
        $resolvedBuild = (Resolve-Path -LiteralPath $buildDir).ProviderPath
        if ($resolvedBuild -ne [IO.Path]::GetFullPath((Join-Path $root "build/$Config"))) {
            throw "Unexpected build directory: $resolvedBuild."
        }
        Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
    }

    $testSetting = if ($NoTests) { 'OFF' } else { 'ON' }
    Invoke-Checked $cmake @('-S', $root, '-B', $buildDir, "-DCMAKE_BUILD_TYPE=$Config", "-DBUILD_TESTING=$testSetting")
    Invoke-Checked $cmake @('--build', $buildDir, '--config', $Config)
    if (-not $NoTests) {
        $testArgs = @('--test-dir', $buildDir, '-C', $Config, '--output-on-failure', '--no-tests=error')
        if ($PSBoundParameters.ContainsKey('TestFilter')) { $testArgs += @('-R', $TestFilter) }
        Invoke-Checked $ctest $testArgs
    }
    Write-Output 'RESULT: OK'
}
catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    Write-Output 'RESULT: FAILED'
    exit 1
}
