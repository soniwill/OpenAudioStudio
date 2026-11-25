[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('\Av[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9]+(?:[.-][A-Za-z0-9]+)*)?\z')]
    [string]$Version
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$bundle = Join-Path $root 'build/Studio_Bundle_Release'
$output = Join-Path $root "build/releases/$Version"
$juce = Join-Path $root 'studio/juce'

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}

function Assert-X64Binary([string]$Path) {
    $reader = [IO.BinaryReader]::new([IO.File]::OpenRead($Path))
    try {
        if ($reader.ReadUInt16() -ne 0x5a4d) { throw "Invalid Windows binary: $Path" }
        $reader.BaseStream.Position = 0x3c
        $reader.BaseStream.Position = $reader.ReadInt32()
        if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) {
            throw "Expected a Windows x64 binary: $Path"
        }
    } finally {
        $reader.Dispose()
    }
}

$commit = Invoke-Checked git @('-C', $root, 'rev-parse', 'HEAD')
$pinnedJuce = Invoke-Checked git @('-C', $root, 'rev-parse', 'HEAD:studio/juce')
$juceCommit = Invoke-Checked git @('-C', $juce, 'rev-parse', 'HEAD')
if ($pinnedJuce -ne $juceCommit) { throw 'Initialize JUCE at the revision pinned by the project.' }
if (Invoke-Checked git @('-C', $root, 'status', '--porcelain', '--untracked-files=no', '--ignore-submodules=untracked')) {
    throw 'Commit tracked changes before packaging a release.'
}
if (Test-Path -LiteralPath $output) { throw "Release directory already exists: $output" }

$executables = @('OpenAudio Studio.exe', 'oae_audio.dll', 'oae_bankc.exe')
foreach ($file in $executables) { Assert-X64Binary (Join-Path $bundle $file) }

$tempRoot = (Resolve-Path -LiteralPath (Join-Path $root 'build')).Path
$scratchName = "release-" + [guid]::NewGuid().ToString('N').Substring(0, 12)
$scratch = Join-Path $tempRoot $scratchName
$sourceName = "OpenAudioStudio-$Version-source"
$binaryName = "OpenAudioStudio-$Version-windows-x64"
$source = Join-Path $scratch $sourceName
$binary = Join-Path $scratch $binaryName
New-Item -ItemType Directory -Path $source, $binary | Out-Null
try {
    $projectTar = Join-Path $scratch 'project.tar'
    $juceTar = Join-Path $scratch 'juce.tar'
    Invoke-Checked git @('-C', $root, 'archive', '--format=tar', "--output=$projectTar", $commit)
    Invoke-Checked git @('-C', $juce, 'archive', '--format=tar', "--output=$juceTar", '--prefix=studio/juce/', $juceCommit)
    Invoke-Checked tar.exe @('-xf', $projectTar, '-C', $source)
    Invoke-Checked tar.exe @('-xf', $juceTar, '-C', $source)

    foreach ($file in $executables + @('README.txt', 'docs')) {
        Copy-Item -LiteralPath (Join-Path $bundle $file) -Destination $binary -Recurse
    }
    Copy-Item -LiteralPath (Join-Path $source 'example_project') -Destination $binary -Recurse
    $playground = Join-Path $binary 'example_project/audio-playground'
    $compiledAudio = Join-Path $scratch 'compiled-playground'
    & (Join-Path $root 'scripts/prepare_playground_audio.ps1') `
        -BankCompiler (Join-Path $binary 'oae_bankc.exe') `
        -OutputDirectory $compiledAudio
    $playgroundBanks = New-Item -ItemType Directory -Path (Join-Path $playground 'build')
    foreach ($file in @('playground.bank', 'playground.bank.catalog.json', 'playground_vo.bank', 'playground_vo.bank.catalog.json')) {
        Copy-Item -LiteralPath (Join-Path $compiledAudio $file) -Destination $playgroundBanks.FullName
    }
    $instructions = Join-Path $binary 'README.txt'
    @'

AUDIOPLAYGROUND IN THE STUDIO:
  - Open example_project/audio-playground/playground_project.json for effects and music.
  - Open example_project/audio-playground/playground_vo_project.json for the dialogue tracks.
  - The example includes the approved WAVs, credits and both banks under audio-playground/build/.
'@ | Add-Content -LiteralPath $instructions -Encoding utf8

    $notices = Join-Path $binary 'licenses'
    New-Item -ItemType Directory -Path $notices | Out-Null
    Copy-Item -LiteralPath (Join-Path $source 'LICENSE') -Destination (Join-Path $notices 'Apache-2.0.txt')
    Copy-Item -LiteralPath (Join-Path $source 'studio/LICENSE') -Destination (Join-Path $notices 'AGPL-3.0.txt')
    Copy-Item -LiteralPath (Join-Path $source 'native/external/licenses') -Destination (Join-Path $notices 'native') -Recurse
    Copy-Item -LiteralPath (Join-Path $source 'unity/com.openaudio.core/THIRD PARTY NOTICES.md') -Destination $notices
    Copy-Item -LiteralPath (Join-Path $source 'studio/juce/LICENSE.md') -Destination (Join-Path $notices 'JUCE.md')
    $juceNotices = Invoke-Checked git @('-C', $juce, 'ls-files', 'modules')
    foreach ($path in ($juceNotices | Where-Object { $_ -match '(?i)(^|/)[^/]*(LICENSE|LICENCE|COPYING|NOTICE)[^/]*$' })) {
        $target = Join-Path $notices "JUCE/$path"
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $source "studio/juce/$path") -Destination $target
    }

    @"
OpenAudio Studio $Version
Commit: $commit
JUCE commit: $juceCommit
Corresponding source: $sourceName.zip, distributed alongside this archive.
Studio: AGPL-3.0-only. Engine and bank compiler: Apache-2.0.
Third-party terms and license texts are under licenses/.
"@ | Set-Content -LiteralPath (Join-Path $binary 'SOURCE.txt') -Encoding utf8

    New-Item -ItemType Directory -Path $output -Force | Out-Null
    foreach ($name in @($binaryName, $sourceName)) {
        $zip = Join-Path $output "$name.zip"
        Invoke-Checked tar.exe @('-a', '-cf', $zip, '-C', $scratch, $name)
    }
    Get-ChildItem -LiteralPath $output -Filter '*.zip' | Sort-Object Name | ForEach-Object {
        "$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant())  $($_.Name)"
    } | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Encoding ascii
    Write-Host "Release assets are in $output"
} finally {
    $resolved = (Resolve-Path -LiteralPath $scratch).Path
    if ($resolved -ne (Join-Path $tempRoot $scratchName)) { throw 'Unexpected release staging path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
