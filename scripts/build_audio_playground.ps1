[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('\Av[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9]+(?:[.-][A-Za-z0-9]+)*)?\z')]
    [string]$Version,
    [string]$RuntimeDirectory,
    [string]$UnityPath
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $RuntimeDirectory) { $RuntimeDirectory = Join-Path $root 'build/Studio_Bundle_Release' }
if (-not $UnityPath) {
    $UnityPath = 'C:/Program Files/Unity/Hub/Editor/6000.0.34f1/Editor/Unity.exe'
}
$editor = (Resolve-Path -LiteralPath $UnityPath).Path
$runtime = (Resolve-Path -LiteralPath (Join-Path $RuntimeDirectory 'oae_audio.dll')).Path
$compiler = (Resolve-Path -LiteralPath (Join-Path $RuntimeDirectory 'oae_bankc.exe')).Path
$unityProject = Join-Path $root 'unity/OpenAudioUnity'
$player = Join-Path $root "build/playground-player-$Version"
$log = Join-Path $root "build/audio-playground-$Version.log"

if (Test-Path -LiteralPath $player) { throw "Player directory already exists: $player" }
$banks = Join-Path $root "build/playground-banks-$Version"
& (Join-Path $PSScriptRoot 'prepare_playground_audio.ps1') -BankCompiler $compiler -OutputDirectory $banks
foreach ($directory in @((Join-Path $unityProject 'Assets/StreamingAssets'), (Join-Path $root 'example_project'))) {
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    foreach ($file in @('playground.bank', 'playground.bank.catalog.json', 'playground_vo.bank', 'playground_vo.bank.catalog.json')) {
        Copy-Item -LiteralPath (Join-Path $banks $file) -Destination $directory -Force
    }
}
Copy-Item -LiteralPath $runtime -Destination (Join-Path $root 'unity/com.openaudio.core/Plugins/x86_64/oae_audio.dll') -Force

$oldPlayerPath = $env:OAE_PLAYGROUND_PLAYER_PATH
$env:OAE_PLAYGROUND_PLAYER_PATH = Join-Path $player 'AudioPlayground.exe'
try {
    $arguments = @('-batchmode', '-nographics', '-quit', '-projectPath', "`"$unityProject`"",
        '-buildTarget', 'StandaloneWindows64', '-executeMethod', 'BuildAudioPlayground.WindowsX64',
        '-logFile', "`"$log`"")
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $env:OAE_PLAYGROUND_PLAYER_PATH)) {
        if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Tail 50 }
        throw "Unity build failed with exit code $($process.ExitCode)."
    }
} finally {
    $env:OAE_PLAYGROUND_PLAYER_PATH = $oldPlayerPath
}

foreach ($file in @('playground.bank', 'playground.bank.catalog.json', 'playground_vo.bank', 'playground_vo.bank.catalog.json')) {
    $packed = Join-Path $player "AudioPlayground_Data/StreamingAssets/$file"
    if ((Get-FileHash -LiteralPath $packed).Hash -ne (Get-FileHash -LiteralPath (Join-Path $banks $file)).Hash) {
        throw "The Unity build did not embed the approved bank or catalog: $file"
    }
}

& (Join-Path $PSScriptRoot 'package_unity_release.ps1') -Version $Version -RuntimePath $runtime -PlayerDirectory $player
