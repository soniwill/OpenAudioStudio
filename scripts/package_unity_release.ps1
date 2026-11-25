[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('\Av[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9]+(?:[.-][A-Za-z0-9]+)*)?\z')]
    [string]$Version,
    [string]$RuntimePath,
    [string]$PlayerDirectory,
    [switch]$IntegrationOnly
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$player = if ($PlayerDirectory) { $PlayerDirectory } else { Join-Path $root 'build/playground-player' }
$output = Join-Path $root "build/unity-releases/$Version"
$packagePath = 'unity/com.openaudio.core'
$package = Join-Path $root $packagePath
$plugin = 'Plugins/x86_64/oae_audio.dll'
$runtime = if ($RuntimePath) { (Resolve-Path -LiteralPath $RuntimePath).Path } else { "$package/$plugin" }
if (-not (Test-Path -LiteralPath $runtime -PathType Leaf)) { throw "Missing Unity runtime: $runtime" }
if (-not $IntegrationOnly) {
    foreach ($file in @(
        "$player/AudioPlayground.exe",
        "$player/UnityPlayer.dll",
        "$player/AudioPlayground_Data/StreamingAssets/playground.bank",
        "$player/AudioPlayground_Data/StreamingAssets/playground_vo.bank",
        "$player/AudioPlayground_Data/StreamingAssets/playground.bank.catalog.json",
        "$player/AudioPlayground_Data/StreamingAssets/playground_vo.bank.catalog.json"
    )) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing build artifact: $file" }
    }
    $packagedPlugin = @(Get-ChildItem -LiteralPath "$player/AudioPlayground_Data/Plugins" -Filter oae_audio.dll -File -Recurse)
    if ($packagedPlugin.Count -ne 1 -or
        (Get-FileHash -LiteralPath $packagedPlugin[0].FullName).Hash -ne (Get-FileHash -LiteralPath $runtime).Hash) {
        throw 'The player and the integration package must use the same native runtime.'
    }
}

if (Test-Path -LiteralPath $output) { throw "Release directory already exists: $output" }
New-Item -ItemType Directory -Path $output | Out-Null
$staging = Join-Path $output 'staging'
New-Item -ItemType Directory -Path "$staging/com.openaudio.core" | Out-Null

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}

$files = Invoke-Checked git @('-C', $root, '-c', 'core.quotepath=false', 'ls-files', '--', "$packagePath/")
foreach ($path in $files) {
    $relative = $path.Substring($packagePath.Length + 1)
    $target = Join-Path "$staging/com.openaudio.core" $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $path) -Destination $target
}
Copy-Item -LiteralPath $runtime -Destination "$staging/com.openaudio.core/$plugin"
Invoke-Checked tar.exe @('-a', '-cf', "$output/OpenAudioUnity-$Version-windows-x64.zip", '-C', $staging, 'com.openaudio.core')

if ($IntegrationOnly) {
    Write-Host "Unity integration package is in $output"
    return
}

$demo = Join-Path $staging 'AudioPlayground'
New-Item -ItemType Directory -Path $demo | Out-Null
Get-ChildItem -LiteralPath $player -Force | Where-Object {
    $_.Name -notmatch 'DoNotShip|DontShip' -and $_.Extension -ne '.pdb'
} | Copy-Item -Destination $demo -Recurse
foreach ($file in @('LICENSE.md', 'THIRD PARTY NOTICES.md')) {
    Copy-Item -LiteralPath (Join-Path $package $file) -Destination $demo
}
foreach ($file in @('AUDIO-CREDITS.md', 'audio-provenance.json', 'voice-provenance.json')) {
    Copy-Item -LiteralPath (Join-Path $root "example_project/audio-playground/$file") -Destination $demo
}
@'
AudioPlayground for Windows x64

Extract the complete ZIP and run AudioPlayground.exe.
Move with WASD or the arrow keys, and look with the mouse. Keys 1/2/3
change footstep surfaces. C toggles combat, F the forest, V the cave,
and E triggers the next synthesized dialogue sample.

The license and notices included here cover the OpenAudio code and runtime
dependencies. The included Unity runtime is distributed under Unity's terms.
'@ | Set-Content -LiteralPath (Join-Path $demo 'README.txt') -Encoding utf8

Invoke-Checked tar.exe @('-a', '-cf', "$output/OpenAudioStudio-AudioPlayground-$Version-windows-x64.zip", '-C', $staging, 'AudioPlayground')
Get-ChildItem -LiteralPath $output -Filter '*.zip' | Sort-Object Name | ForEach-Object {
    "$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant())  $($_.Name)"
} | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS-unity.txt') -Encoding ascii
Write-Host "Unity release assets are in $output"
