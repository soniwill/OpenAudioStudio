[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BankCompiler,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$SourceDirectory = (Join-Path $PSScriptRoot '../example_project/audio-playground')
)

$ErrorActionPreference = 'Stop'
$compiler = (Resolve-Path -LiteralPath $BankCompiler).Path
$source = (Resolve-Path -LiteralPath $SourceDirectory).Path
$main = Get-Content -LiteralPath (Join-Path $source 'audio-provenance.json') -Raw | ConvertFrom-Json
$voices = Get-Content -LiteralPath (Join-Path $source 'voice-provenance.json') -Raw | ConvertFrom-Json
$audio = @($main) + @($voices.files)
if ($main.Count -ne 21 -or $voices.files.Count -ne 3 -or
    @($audio.file | Sort-Object -Unique).Count -ne $audio.Count) {
    throw 'Expected 21 effects and music samples and three distinct dialogue files.'
}

foreach ($entry in $audio) {
    if ($entry.file -notmatch '^[a-z0-9_]+\.wav$' -or $entry.sha256 -notmatch '^[0-9a-f]{64}$') {
        throw 'Invalid audio provenance entry.'
    }
    $file = Join-Path $source "assets/playground/$($entry.file)"
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw "Audio does not match the approved WAV: $($entry.file)"
    }
}

$projects = [ordered]@{
    'playground_project.json' = @{ Bank = 'playground.bank'; Samples = @($main.file) }
    'playground_vo_project.json' = @{ Bank = 'playground_vo.bank'; Samples = @($voices.files.file) }
}
foreach ($project in $projects.GetEnumerator()) {
    $file = Join-Path $source $project.Key
    $data = Get-Content -LiteralPath $file -Raw | ConvertFrom-Json
    $expected = @($project.Value.Samples | ForEach-Object { "assets/playground/$_" })
    if (Compare-Object -ReferenceObject $expected -DifferenceObject @($data.samples.file)) {
        throw "Project samples do not match the approved inputs: $($project.Key)"
    }
}

[IO.Directory]::CreateDirectory([IO.Path]::GetFullPath($OutputDirectory)) | Out-Null
foreach ($project in $projects.GetEnumerator()) {
    $bank = Join-Path $OutputDirectory $project.Value.Bank
    & $compiler (Join-Path $source $project.Key) -o $bank
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath "$bank.catalog.json" -PathType Leaf)) {
        throw "Bank or catalog generation failed: $bank"
    }
}
