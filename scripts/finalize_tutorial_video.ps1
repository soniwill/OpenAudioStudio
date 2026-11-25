<#
.SYNOPSIS
    Pos-processa gravacoes brutas de tutoriais do Audio Playground usando ffmpeg local gratuito.

.DESCRIPTION
    Aplica normalizacao de loudness em duas passadas no padrao YouTube (-14 LUFS, -1.0 dBTP,
    LRA 11) com audio AAC 48 kHz a 192 kbps. Suporta recorte opcional (-Start / -End),
    queima opcional de legendas SRT (-Subtitles) e modo -WhatIf sem escrita em disco.
#>
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = "Low")]
param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [string]$OutputPath,

    [string]$Start,

    [string]$End,

    [string]$Subtitles,

    [string]$FfmpegPath = "ffmpeg",

    [string]$FfprobePath = "ffprobe",

    [double]$TargetI = -14.0,

    [double]$TargetTP = -1.0,

    [double]$TargetLRA = 11.0
)

$ErrorActionPreference = "Stop"

function Resolve-ToolPath {
    param([string]$Name)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $cmd) {
        throw "Ferramenta '$Name' nao encontrada no PATH. Instale via winget ou informe -FfmpegPath / -FfprobePath."
    }
    return $cmd.Source
}

function ConvertTo-FfmpegSubtitlePath {
    param([string]$RawPath)
    $full = [System.IO.Path]::GetFullPath($RawPath)
    $escaped = $full -replace '\\', '/'
    $escaped = $escaped -replace ':', '\:'
    $escaped = $escaped -replace "'", "\'"
    return $escaped
}

$resolvedFfmpeg = Resolve-ToolPath -Name $FfmpegPath
$null = Resolve-ToolPath -Name $FfprobePath

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $dir = [System.IO.Path]::GetDirectoryName($InputPath)
    $base = [System.IO.Path]::GetFileNameWithoutExtension($InputPath)
    $OutputPath = if ([string]::IsNullOrWhiteSpace($dir)) {
        "${base}_final.mp4"
    } else {
        Join-Path $dir "${base}_final.mp4"
    }
}

$trimArgs = @()
if (-not [string]::IsNullOrWhiteSpace($Start)) {
    $trimArgs += @("-ss", $Start)
}
if (-not [string]::IsNullOrWhiteSpace($End)) {
    $trimArgs += @("-to", $End)
}

$invCulture = [System.Globalization.CultureInfo]::InvariantCulture
$targetIStr = $TargetI.ToString("0.0", $invCulture)
$targetTPStr = $TargetTP.ToString("0.0", $invCulture)
$targetLRAStr = $TargetLRA.ToString("0.0", $invCulture)

$pass1Filter = "loudnorm=I=${targetIStr}:TP=${targetTPStr}:LRA=${targetLRAStr}:print_format=json"
$pass1Args = @("-hide_banner", "-y") + $trimArgs + @(
    "-i", $InputPath,
    "-vn",
    "-af", $pass1Filter,
    "-f", "null",
    "NUL"
)

$needsVideoEncode = (-not [string]::IsNullOrWhiteSpace($Subtitles)) -or ($trimArgs.Count -gt 0)
$videoArgs = if ($needsVideoEncode) {
    $v = @()
    if (-not [string]::IsNullOrWhiteSpace($Subtitles)) {
        $subEscaped = ConvertTo-FfmpegSubtitlePath -RawPath $Subtitles
        $v += @("-vf", "subtitles='${subEscaped}'")
    }
    $v += @("-c:v", "libx264", "-preset", "medium", "-crf", "18", "-pix_fmt", "yuv420p")
    $v
} else {
    @("-c:v", "copy")
}

$plannedPass2Filter = "loudnorm=I=${targetIStr}:TP=${targetTPStr}:LRA=${targetLRAStr}:measured_I=<M_I>:measured_TP=<M_TP>:measured_LRA=<M_LRA>:measured_thresh=<M_THRESH>:offset=<OFFSET>:linear=true:print_format=summary"

if (-not $PSCmdlet.ShouldProcess($OutputPath, "Normalizar audio para $targetIStr LUFS / 48 kHz AAC")) {
    Write-Host "[finalize_tutorial_video] -WhatIf ativo. Nenhuma escrita sera executada."
    Write-Host "[Passada 1] $resolvedFfmpeg $($pass1Args -join ' ')"
    Write-Host "[Passada 2] $resolvedFfmpeg -hide_banner -y $($trimArgs -join ' ') -i `"$InputPath`" $($videoArgs -join ' ') -af `"$plannedPass2Filter`" -ar 48000 -ac 2 -c:a aac -b:a 192k -movflags +faststart `"$OutputPath`""
    exit 0
}

if (-not (Test-Path -LiteralPath $InputPath)) {
    throw "Arquivo de entrada nao encontrado: $InputPath"
}
if (-not [string]::IsNullOrWhiteSpace($Subtitles) -and -not (Test-Path -LiteralPath $Subtitles)) {
    throw "Arquivo de legendas nao encontrado: $Subtitles"
}

$outDir = [System.IO.Path]::GetDirectoryName([System.IO.Path]::GetFullPath($OutputPath))
if (-not [string]::IsNullOrWhiteSpace($outDir) -and -not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

Write-Host "[finalize_tutorial_video] Passada 1/2: medindo loudness de '$InputPath'..."
$prevEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$pass1Output = & $resolvedFfmpeg @pass1Args 2>&1
$pass1Exit = $LASTEXITCODE
$ErrorActionPreference = $prevEap

if ($pass1Exit -ne 0) {
    throw "ffmpeg Passada 1 falhou (exit $pass1Exit): $($pass1Output -join "`n")"
}

$pass1Text = ($pass1Output | ForEach-Object { $_.ToString() }) -join "`n"
$jsonMatch = [regex]::Match($pass1Text, '(?s)\{\s*"input_i"\s*:\s*.*?\}')
if (-not $jsonMatch.Success) {
    throw "Nao foi possivel extrair estatisticas JSON de loudnorm na Passada 1."
}

$stats = $jsonMatch.Value | ConvertFrom-Json
if ($stats.input_i -eq "-inf" -or $stats.input_tp -eq "-inf") {
    Write-Warning "[finalize_tutorial_video] Audio silencioso (-inf); aplicando loudnorm de 1 passada."
    $pass2Filter = "loudnorm=I=${targetIStr}:TP=${targetTPStr}:LRA=${targetLRAStr}:print_format=summary"
} else {
    $pass2Filter = "loudnorm=I=${targetIStr}:TP=${targetTPStr}:LRA=${targetLRAStr}:measured_I=$($stats.input_i):measured_TP=$($stats.input_tp):measured_LRA=$($stats.input_lra):measured_thresh=$($stats.input_thresh):offset=$($stats.target_offset):linear=true:print_format=summary"
}

$pass2Args = @("-hide_banner", "-y") + $trimArgs + @(
    "-i", $InputPath
) + $videoArgs + @(
    "-af", $pass2Filter,
    "-ar", "48000",
    "-ac", "2",
    "-c:a", "aac",
    "-b:a", "192k",
    "-movflags", "+faststart",
    $OutputPath
)

Write-Host "[finalize_tutorial_video] Passada 2/2: gerando '$OutputPath' ($targetIStr LUFS, 48 kHz AAC 192 kbps)..."
$ErrorActionPreference = "Continue"
$pass2Output = & $resolvedFfmpeg @pass2Args 2>&1
$pass2Exit = $LASTEXITCODE
$ErrorActionPreference = $prevEap

if ($pass2Exit -ne 0) {
    throw "ffmpeg Passada 2 falhou (exit $pass2Exit): $($pass2Output -join "`n")"
}

Write-Host "[finalize_tutorial_video] Concluido com sucesso: $OutputPath"
exit 0
