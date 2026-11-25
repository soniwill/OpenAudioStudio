# scripts/create_playground_assets.ps1
# Acquires/synthesizes 48 kHz 16-bit PCM Mono WAV assets, generates playground JSON projects,
# compiles playground.bank & playground_vo.bank via oae_bankc, and deploys to Unity StreamingAssets.

[CmdletBinding()]
param(
    [switch]$Offline,
    [switch]$SyntheticOnly
)

$ErrorActionPreference = "Stop"
if ($SyntheticOnly) { $Offline = $true }

$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$targetDir = Join-Path $repoRoot "example_project\assets\playground"
$saDir = Join-Path $repoRoot "unity\OpenAudioUnity\Assets\StreamingAssets"
$tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "oae_playground_tmp"

foreach ($dir in @($targetDir, $saDir, $tempDir)) {
    if (-not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
}

$ffmpegCmd = Get-Command ffmpeg -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1

function Write-WavFile {
    param(
        [string]$FilePath,
        [float[]]$Samples,
        [int]$SampleRate = 48000
    )

    $numSamples = $Samples.Length
    $numChannels = 1
    $bitsPerSample = 16
    $byteRate = $SampleRate * $numChannels * ($bitsPerSample / 8)
    $blockAlign = $numChannels * ($bitsPerSample / 8)
    $dataSize = $numSamples * ($bitsPerSample / 8)
    $chunkSize = 36 + $dataSize

    $fs = [System.IO.File]::Open($FilePath, [System.IO.FileMode]::Create)
    $bw = New-Object System.IO.BinaryWriter($fs)

    try {
        $bw.Write([System.Text.Encoding]::ASCII.GetBytes("RIFF"))
        $bw.Write([uint32]$chunkSize)
        $bw.Write([System.Text.Encoding]::ASCII.GetBytes("WAVE"))

        $bw.Write([System.Text.Encoding]::ASCII.GetBytes("fmt "))
        $bw.Write([uint32]16)
        $bw.Write([uint16]1)
        $bw.Write([uint16]$numChannels)
        $bw.Write([uint32]$SampleRate)
        $bw.Write([uint32]$byteRate)
        $bw.Write([uint16]$blockAlign)
        $bw.Write([uint16]$bitsPerSample)

        $bw.Write([System.Text.Encoding]::ASCII.GetBytes("data"))
        $bw.Write([uint32]$dataSize)

        for ($i = 0; $i -lt $numSamples; $i++) {
            $val = [Math]::Max(-1.0, [Math]::Min(1.0, $Samples[$i]))
            $sample16 = [int16]($val * 32767.0)
            $bw.Write($sample16)
        }
    }
    finally {
        $bw.Close()
        $fs.Close()
    }
}

function Convert-WithFfmpegTo48kMono16 {
    param(
        [string]$InputPath,
        [string]$OutputPath,
        [double]$StartSeconds = 0.0,
        [double]$DurationSeconds = 0.0,
        [string]$AudioFilter = ""
    )

    if (-not $ffmpegCmd -or -not (Test-Path -LiteralPath $InputPath)) {
        return $false
    }

    $ffArgs = [System.Collections.Generic.List[string]]::new()
    $ffArgs.Add("-y")
    $ffArgs.Add("-loglevel")
    $ffArgs.Add("error")
    if ($StartSeconds -gt 0.0) {
        $ffArgs.Add("-ss")
        $ffArgs.Add($StartSeconds.ToString([System.Globalization.CultureInfo]::InvariantCulture))
    }
    $ffArgs.Add("-i")
    $ffArgs.Add($InputPath)
    if ($DurationSeconds -gt 0.0) {
        $ffArgs.Add("-t")
        $ffArgs.Add($DurationSeconds.ToString([System.Globalization.CultureInfo]::InvariantCulture))
    }
    if (-not [string]::IsNullOrWhiteSpace($AudioFilter)) {
        $ffArgs.Add("-af")
        $ffArgs.Add($AudioFilter)
    }
    $ffArgs.Add("-ar")
    $ffArgs.Add("48000")
    $ffArgs.Add("-ac")
    $ffArgs.Add("1")
    $ffArgs.Add("-sample_fmt")
    $ffArgs.Add("s16")
    $ffArgs.Add("-c:a")
    $ffArgs.Add("pcm_s16le")
    $ffArgs.Add($OutputPath)

    $oldPref = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $ffmpegCmd.Source @ffArgs
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $OutputPath)) {
            return $false
        }
        $info = Get-Item -LiteralPath $OutputPath
        return ($info.Length -gt 1024)
    }
    catch {
        return $false
    }
    finally {
        $ErrorActionPreference = $oldPref
    }
}

$script:networkAvailable = -not $Offline.IsPresent
$script:hostFailures = @{}
$script:disabledHosts = @{}

function Try-DownloadCc0Sample {
    param(
        [string]$Url,
        [string]$OutputPath,
        [double]$StartSeconds = 0.0,
        [double]$DurationSeconds = 0.0,
        [string]$AudioFilter = ""
    )

    if (-not $script:networkAvailable -or -not $ffmpegCmd -or [string]::IsNullOrWhiteSpace($Url)) {
        return $false
    }

    $uriObj = $null
    try {
        $uriObj = [Uri]$Url
    }
    catch {
        return $false
    }

    $targetHost = $uriObj.Host.ToLowerInvariant()
    if ($script:disabledHosts.ContainsKey($targetHost)) {
        return $false
    }

    $ext = [System.IO.Path]::GetExtension($uriObj.AbsolutePath)
    if ([string]::IsNullOrWhiteSpace($ext)) {
        $ext = ".tmp"
    }
    $tmpDownload = Join-Path $tempDir ([System.Guid]::NewGuid().ToString("N") + $ext)

    try {
        if ($targetHost -eq "upload.wikimedia.org") {
            Start-Sleep -Milliseconds 250
        }
        Invoke-WebRequest -Uri $Url -OutFile $tmpDownload -TimeoutSec 25 -UseBasicParsing -Headers @{ "User-Agent" = "OpenAudioStudio/1.0 (https://github.com/openaudiostudio)" }
        if (Convert-WithFfmpegTo48kMono16 -InputPath $tmpDownload -OutputPath $OutputPath -StartSeconds $StartSeconds -DurationSeconds $DurationSeconds -AudioFilter $AudioFilter) {
            $script:hostFailures[$targetHost] = 0
            return $true
        }
        $script:hostFailures[$targetHost] = ($script:hostFailures[$targetHost] -as [int]) + 1
    }
    catch {
        $script:hostFailures[$targetHost] = ($script:hostFailures[$targetHost] -as [int]) + 1
    }
    finally {
        if (Test-Path -LiteralPath $tmpDownload) {
            Remove-Item -LiteralPath $tmpDownload -Force -ErrorAction SilentlyContinue
        }
    }

    if (($script:hostFailures[$targetHost] -as [int]) -ge 3) {
        $script:disabledHosts[$targetHost] = $true
    }
    return $false
}

function Try-SynthesizeNeuralVoiceover {
    param(
        [string]$Text,
        [string]$Voice,
        [string]$OutputPath
    )

    if ($Offline.IsPresent -or -not $ffmpegCmd -or [string]::IsNullOrWhiteSpace($Text) -or [string]::IsNullOrWhiteSpace($Voice)) {
        return $false
    }

    $edgeTtsCmd = Get-Command edge-tts -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    $uvxCmd = Get-Command uvx -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $edgeTtsCmd -and -not $uvxCmd) {
        return $false
    }

    $tmpMp3 = Join-Path $tempDir ([System.Guid]::NewGuid().ToString("N") + "_edge.mp3")
    try {
        $exe = $null
        $argList = $null
        if ($edgeTtsCmd) {
            $exe = $edgeTtsCmd.Source
            $argList = @("--voice", $Voice, "--text", "`"$Text`"", "--write-media", "`"$tmpMp3`"")
        } else {
            $exe = $uvxCmd.Source
            $argList = @("--from", "edge-tts", "edge-tts", "--voice", $Voice, "--text", "`"$Text`"", "--write-media", "`"$tmpMp3`"")
        }

        $proc = Start-Process -FilePath $exe -ArgumentList $argList -Wait -PassThru -NoNewWindow
        if ($proc.ExitCode -eq 0 -and (Test-Path -LiteralPath $tmpMp3)) {
            $mp3Info = Get-Item -LiteralPath $tmpMp3
            if ($mp3Info.Length -gt 1024) {
                if (Convert-WithFfmpegTo48kMono16 -InputPath $tmpMp3 -OutputPath $OutputPath -AudioFilter "silenceremove=start_periods=1:start_silence=0.02:start_threshold=-45dB") {
                    return $true
                }
            }
        }
        return $false
    }
    catch {
        return $false
    }
    finally {
        if (Test-Path -LiteralPath $tmpMp3) {
            Remove-Item -LiteralPath $tmpMp3 -Force -ErrorAction SilentlyContinue
        }
    }
}

function Try-SynthesizeVoiceover {
    param(
        [string]$Text,
        [string]$OutputPath
    )

    $tmpWav = Join-Path $tempDir ([System.Guid]::NewGuid().ToString("N") + "_sapi.wav")
    $synth = $null
    try {
        Add-Type -AssemblyName System.Speech -ErrorAction Stop
        $synth = New-Object System.Speech.Synthesis.SpeechSynthesizer
        $synth.Rate = 0
        $synth.Volume = 100
        $synth.SetOutputToWaveFile($tmpWav)
        $synth.Speak($Text)
        $synth.SetOutputToNull()

        if (Convert-WithFfmpegTo48kMono16 -InputPath $tmpWav -OutputPath $OutputPath) {
            return $true
        }

        if (Test-Path -LiteralPath $tmpWav) {
            $bytes = [System.IO.File]::ReadAllBytes($tmpWav)
            if ($bytes.Length -gt 2048) {
                Copy-Item -LiteralPath $tmpWav -Destination $OutputPath -Force
                return $true
            }
        }
        return $false
    }
    catch {
        return $false
    }
    finally {
        if ($synth -ne $null) {
            $synth.Dispose()
        }
        if (Test-Path -LiteralPath $tmpWav) {
            Remove-Item -LiteralPath $tmpWav -Force -ErrorAction SilentlyContinue
        }
    }
}

$sampleRate = 48000
$twoPi = 2.0 * [Math]::PI

$cc0Sources = @{
    "step_wood_1.wav"         = @{ Url = "https://raw.githubusercontent.com/Boyquotes/kenney-impact-sounds-for-godot/main/addons/kenney%20impact%20sounds/footstep_wood_000.ogg"; Start = 0.0; Duration = 0.0; Filter = "volume=1.6" }
    "step_wood_2.wav"         = @{ Url = "https://raw.githubusercontent.com/Boyquotes/kenney-impact-sounds-for-godot/main/addons/kenney%20impact%20sounds/footstep_wood_001.ogg"; Start = 0.0; Duration = 0.0; Filter = "volume=1.6" }
    "step_wood_3.wav"         = @{ Url = "https://raw.githubusercontent.com/Boyquotes/kenney-impact-sounds-for-godot/main/addons/kenney%20impact%20sounds/footstep_wood_002.ogg"; Start = 0.0; Duration = 0.0; Filter = "volume=1.6" }
    "step_wood_4.wav"         = @{ Url = "https://raw.githubusercontent.com/Boyquotes/kenney-impact-sounds-for-godot/main/addons/kenney%20impact%20sounds/footstep_wood_003.ogg"; Start = 0.0; Duration = 0.0; Filter = "volume=1.6" }
    "step_concrete_1.wav"     = @{ Url = "https://raw.githubusercontent.com/Boyquotes/kenney-impact-sounds-for-godot/main/addons/kenney%20impact%20sounds/footstep_concrete_000.ogg"; Start = 0.0; Duration = 0.0; Filter = "volume=1.6" }
    "step_concrete_2.wav"     = @{ Url = "https://raw.githubusercontent.com/Boyquotes/kenney-impact-sounds-for-godot/main/addons/kenney%20impact%20sounds/footstep_concrete_001.ogg"; Start = 0.0; Duration = 0.0; Filter = "volume=1.6" }
    "step_concrete_3.wav"     = @{ Url = "https://raw.githubusercontent.com/Boyquotes/kenney-impact-sounds-for-godot/main/addons/kenney%20impact%20sounds/footstep_concrete_002.ogg"; Start = 0.0; Duration = 0.0; Filter = "volume=1.6" }
    "step_concrete_4.wav"     = @{ Url = "https://raw.githubusercontent.com/Boyquotes/kenney-impact-sounds-for-godot/main/addons/kenney%20impact%20sounds/footstep_concrete_003.ogg"; Start = 0.0; Duration = 0.0; Filter = "volume=1.6" }
    "step_grass_1.wav"        = @{ Url = "https://raw.githubusercontent.com/Boyquotes/kenney-impact-sounds-for-godot/main/addons/kenney%20impact%20sounds/footstep_grass_000.ogg"; Start = 0.0; Duration = 0.0; Filter = "volume=1.6" }
    "step_grass_2.wav"        = @{ Url = "https://raw.githubusercontent.com/Boyquotes/kenney-impact-sounds-for-godot/main/addons/kenney%20impact%20sounds/footstep_grass_001.ogg"; Start = 0.0; Duration = 0.0; Filter = "volume=1.6" }
    "step_grass_3.wav"        = @{ Url = "https://raw.githubusercontent.com/Boyquotes/kenney-impact-sounds-for-godot/main/addons/kenney%20impact%20sounds/footstep_grass_002.ogg"; Start = 0.0; Duration = 0.0; Filter = "volume=1.6" }
    "step_grass_4.wav"        = @{ Url = "https://raw.githubusercontent.com/Boyquotes/kenney-impact-sounds-for-godot/main/addons/kenney%20impact%20sounds/footstep_grass_003.ogg"; Start = 0.0; Duration = 0.0; Filter = "volume=1.6" }
    "emitter_3d_loop.wav"     = @{ Url = "https://opengameart.org/sites/default/files/picolojazzguitar.ogg"; Start = 0.6; Duration = 16.0; Filter = "silenceremove=start_periods=1:start_silence=0.02:start_threshold=-40dB,highpass=f=180,afade=t=in:st=0:d=0.02,afade=t=out:st=15.90:d=0.10,volume=1.35" }
    "music_explore.wav"       = @{ Url = "https://opengameart.org/sites/default/files/shades_of_a_forest_loop.flac"; Start = 0.0; Duration = 16.0; Filter = "afade=t=in:st=0:d=0.12,afade=t=out:st=15.85:d=0.15,volume=0.95" }
    "music_combat.wav"        = @{ Url = "https://opengameart.org/sites/default/files/determined_pursuit_loop.wav"; Start = 0.0; Duration = 16.0; Filter = "afade=t=in:st=0:d=0.08,afade=t=out:st=15.88:d=0.12,volume=0.95" }
    "music_tension_layer.wav" = @{ Url = "https://opengameart.org/sites/default/files/Battle.mp3"; Start = 4.0; Duration = 16.0; Filter = "lowpass=f=520,afade=t=in:st=0:d=0.15,afade=t=out:st=15.80:d=0.20,volume=0.80" }
    "forest_bird_1.wav"       = @{ Url = "https://upload.wikimedia.org/wikipedia/commons/e/e5/Bird_call_with_so-called_separated_chirps.ogg"; Start = 0.2; Duration = 1.2; Filter = "silenceremove=start_periods=1:start_silence=0.02:start_threshold=-40dB,afade=t=in:st=0:d=0.02,volume=1.6" }
    "forest_bird_2.wav"       = @{ Url = "https://upload.wikimedia.org/wikipedia/commons/d/d4/Dirty_bird_call.oga"; Start = 0.0; Duration = 1.0; Filter = "silenceremove=start_periods=1:start_silence=0.02:start_threshold=-40dB,afade=t=in:st=0:d=0.02,volume=1.6" }
    "forest_wind_grain.wav"   = @{ Url = "https://upload.wikimedia.org/wikipedia/commons/2/2d/Howling_wind.ogg"; Start = 2.0; Duration = 1.8; Filter = "afade=t=in:st=0:d=0.25,afade=t=out:st=1.55:d=0.25,volume=1.3" }
    "forest_fire_crackle.wav" = @{ Url = "https://opengameart.org/sites/default/files/fire.wav"; Start = 0.5; Duration = 1.6; Filter = "highpass=f=450,afade=t=in:st=0:d=0.15,afade=t=out:st=1.45:d=0.15,volume=1.1" }
    "ui_click.wav"            = @{ Url = "https://raw.githubusercontent.com/Calinou/kenney-ui-audio/master/addons/kenney_ui_audio/click1.wav"; Start = 0.0; Duration = 0.0; Filter = "volume=1.4" }
}

Write-Host "[Playground Assets] Preparing 24 AAA WAV assets (48 kHz 16-bit PCM Mono)..." -ForegroundColor Cyan

function Invoke-Cc0AssetDownload {
    param(
        [string]$FileName,
        [string]$OutputPath
    )
    $spec = $cc0Sources[$FileName]
    if (-not $spec) {
        return $false
    }
    return (Try-DownloadCc0Sample -Url $spec.Url -OutputPath $OutputPath -StartSeconds $spec.Start -DurationSeconds $spec.Duration -AudioFilter $spec.Filter)
}

function Ensure-FootstepAsset {
    param(
        [string]$FileName,
        [double]$DurationSec,
        [double]$BaseFreq,
        [double]$OverFreq,
        [double]$DecayRate,
        [int]$Seed,
        [double]$NoiseMix
    )
    $outPath = Join-Path $targetDir $FileName
    if (Invoke-Cc0AssetDownload -FileName $FileName -OutputPath $outPath) {
        Write-Host "  -> $FileName (CC0 + ffmpeg)"
        return
    }
    $rng = [System.Random]::new($Seed)
    $num = [int]($sampleRate * $DurationSec)
    $buf = New-Object float[] $num
    for ($i = 0; $i -lt $num; $i++) {
        $t = $i / $sampleRate
        $env = [Math]::Exp(-$t * $DecayRate)
        $tone = 0.55 * [Math]::Sin($twoPi * ($BaseFreq * [Math]::Exp(-$t * 8.0) + 40.0) * $t) +
                0.28 * [Math]::Sin($twoPi * $OverFreq * $t)
        $noise = ($rng.NextDouble() * 2.0 - 1.0) * $NoiseMix * [Math]::Exp(-$t * ($DecayRate * 1.3))
        $attack = [Math]::Min(1.0, $t / 0.003)
        $buf[$i] = ($tone * $env + $noise) * $attack
    }
    Write-WavFile -FilePath $outPath -Samples $buf
    Write-Host "  -> $FileName (deterministic synth)"
}

# 1-12. Multi-surface footsteps (4 variations x Wood, Concrete, Grass)
Ensure-FootstepAsset -FileName "step_wood_1.wav"     -DurationSec 0.22 -BaseFreq 160.0 -OverFreq 380.0  -DecayRate 22.0 -Seed 101 -NoiseMix 0.18
Ensure-FootstepAsset -FileName "step_wood_2.wav"     -DurationSec 0.22 -BaseFreq 195.0 -OverFreq 440.0  -DecayRate 24.0 -Seed 102 -NoiseMix 0.16
Ensure-FootstepAsset -FileName "step_wood_3.wav"     -DurationSec 0.22 -BaseFreq 175.0 -OverFreq 410.0  -DecayRate 23.0 -Seed 103 -NoiseMix 0.17
Ensure-FootstepAsset -FileName "step_wood_4.wav"     -DurationSec 0.22 -BaseFreq 210.0 -OverFreq 465.0  -DecayRate 25.0 -Seed 104 -NoiseMix 0.15
Ensure-FootstepAsset -FileName "step_concrete_1.wav" -DurationSec 0.18 -BaseFreq 850.0 -OverFreq 2100.0 -DecayRate 42.0 -Seed 201 -NoiseMix 0.35
Ensure-FootstepAsset -FileName "step_concrete_2.wav" -DurationSec 0.18 -BaseFreq 980.0 -OverFreq 2450.0 -DecayRate 45.0 -Seed 202 -NoiseMix 0.32
Ensure-FootstepAsset -FileName "step_concrete_3.wav" -DurationSec 0.18 -BaseFreq 910.0 -OverFreq 2250.0 -DecayRate 43.0 -Seed 203 -NoiseMix 0.34
Ensure-FootstepAsset -FileName "step_concrete_4.wav" -DurationSec 0.18 -BaseFreq 1040.0 -OverFreq 2550.0 -DecayRate 46.0 -Seed 204 -NoiseMix 0.30
Ensure-FootstepAsset -FileName "step_grass_1.wav"    -DurationSec 0.25 -BaseFreq 260.0 -OverFreq 1250.0 -DecayRate 16.0 -Seed 301 -NoiseMix 0.45
Ensure-FootstepAsset -FileName "step_grass_2.wav"    -DurationSec 0.25 -BaseFreq 310.0 -OverFreq 1420.0 -DecayRate 17.0 -Seed 302 -NoiseMix 0.42
Ensure-FootstepAsset -FileName "step_grass_3.wav"    -DurationSec 0.25 -BaseFreq 285.0 -OverFreq 1330.0 -DecayRate 16.5 -Seed 303 -NoiseMix 0.44
Ensure-FootstepAsset -FileName "step_grass_4.wav"    -DurationSec 0.25 -BaseFreq 330.0 -OverFreq 1490.0 -DecayRate 17.5 -Seed 304 -NoiseMix 0.40

# 7. 3D Emitter beacon loop (4.0s)
$emitterPath = Join-Path $targetDir "emitter_3d_loop.wav"
if (Invoke-Cc0AssetDownload -FileName "emitter_3d_loop.wav" -OutputPath $emitterPath) {
    Write-Host "  -> emitter_3d_loop.wav (CC0 + ffmpeg)"
} else {
    $rng = [System.Random]::new(401)
    $num = [int]($sampleRate * 4.0)
    $buf = New-Object float[] $num
    for ($i = 0; $i -lt $num; $i++) {
        $t = $i / $sampleRate
        $warm = 0.22 * [Math]::Sin($twoPi * 110.0 * $t) + 0.12 * [Math]::Sin($twoPi * 220.0 * $t)
        $pulse = 0.14 * (0.5 + 0.5 * [Math]::Sin($twoPi * 2.0 * $t)) * [Math]::Sin($twoPi * 440.0 * $t)
        $crackle = 0.0
        if ($rng.NextDouble() -lt 0.008) {
            $crackle = 0.45 * ($rng.NextDouble() * 2.0 - 1.0)
        }
        $buf[$i] = $warm + $pulse + $crackle + 0.04 * ($rng.NextDouble() * 2.0 - 1.0)
    }
    for ($i = 0; $i -lt 100; $i++) {
        $a = $i / 100.0
        $buf[$i] *= $a
        $buf[$num - 1 - $i] *= $a
    }
    Write-WavFile -FilePath $emitterPath -Samples $buf
    Write-Host "  -> emitter_3d_loop.wav (deterministic synth)"
}

# 8. Adaptive Music - Exploration (4.0s = 2 bars at 120 BPM)
$explorePath = Join-Path $targetDir "music_explore.wav"
if (Invoke-Cc0AssetDownload -FileName "music_explore.wav" -OutputPath $explorePath) {
    Write-Host "  -> music_explore.wav (CC0 + ffmpeg)"
} else {
    $numMus = [int]($sampleRate * 4.0)
    $exploreBuf = New-Object float[] $numMus
    $exploreBass = @(65.41, 77.78, 98.00, 77.78)
    for ($i = 0; $i -lt $numMus; $i++) {
        $t = $i / $sampleRate
        $stepIdx = [int](($t / 1.0) % 4)
        $stepTime = ($t / 1.0) % 1.0
        $bass = 0.28 * [Math]::Exp(-$stepTime * 3.5) * [Math]::Sin($twoPi * $exploreBass[$stepIdx] * $t)
        $pad = 0.10 * ([Math]::Sin($twoPi * 261.63 * $t) + [Math]::Sin($twoPi * 311.13 * $t) + [Math]::Sin($twoPi * 392.00 * $t))
        $exploreBuf[$i] = $bass + $pad
    }
    for ($i = 0; $i -lt 100; $i++) {
        $a = $i / 100.0
        $exploreBuf[$i] *= $a
        $exploreBuf[$numMus - 1 - $i] *= $a
    }
    Write-WavFile -FilePath $explorePath -Samples $exploreBuf
    Write-Host "  -> music_explore.wav (deterministic synth)"
}

# 9. Adaptive Music - Combat (4.0s = 2 bars at 120 BPM)
$combatPath = Join-Path $targetDir "music_combat.wav"
if (Invoke-Cc0AssetDownload -FileName "music_combat.wav" -OutputPath $combatPath) {
    Write-Host "  -> music_combat.wav (CC0 + ffmpeg)"
} else {
    $numMus = [int]($sampleRate * 4.0)
    $combatBuf = New-Object float[] $numMus
    $combatLead = @(261.63, 392.00, 311.13, 523.25, 466.16, 392.00, 311.13, 349.23)
    for ($i = 0; $i -lt $numMus; $i++) {
        $t = $i / $sampleRate
        $stepIdx = [int](($t / 0.5) % 8)
        $stepTime = ($t / 0.5) % 1.0
        $lead = 0.30 * [Math]::Exp(-$stepTime * 4.5) * ([Math]::Sin($twoPi * $combatLead[$stepIdx] * $t) + 0.4 * [Math]::Sin($twoPi * ($combatLead[$stepIdx] * 2.0) * $t))
        $bassTime = ($t / 0.25) % 1.0
        $bass = 0.34 * [Math]::Exp(-$bassTime * 6.5) * [Math]::Sin($twoPi * 65.41 * $t)
        $combatBuf[$i] = $lead + $bass
    }
    for ($i = 0; $i -lt 100; $i++) {
        $a = $i / 100.0
        $combatBuf[$i] *= $a
        $combatBuf[$numMus - 1 - $i] *= $a
    }
    Write-WavFile -FilePath $combatPath -Samples $combatBuf
    Write-Host "  -> music_combat.wav (deterministic synth)"
}

# 9b. Adaptive Music - Tension Layer (16.0s orchestral low pulse)
$tensionPath = Join-Path $targetDir "music_tension_layer.wav"
if (Invoke-Cc0AssetDownload -FileName "music_tension_layer.wav" -OutputPath $tensionPath) {
    Write-Host "  -> music_tension_layer.wav (CC0 + ffmpeg)"
} else {
    $numMus = [int]($sampleRate * 4.0)
    $tensionBuf = New-Object float[] $numMus
    for ($i = 0; $i -lt $numMus; $i++) {
        $t = $i / $sampleRate
        $pulse = [Math]::Exp(-(($t % 0.5) * 6.0))
        $tensionBuf[$i] = 0.38 * $pulse * [Math]::Sin($twoPi * 55.0 * $t)
    }
    Write-WavFile -FilePath $tensionPath -Samples $tensionBuf
    Write-Host "  -> music_tension_layer.wav (deterministic synth)"
}

# 10-12. Forest Scatterer Grains
$bird1Path = Join-Path $targetDir "forest_bird_1.wav"
if (Invoke-Cc0AssetDownload -FileName "forest_bird_1.wav" -OutputPath $bird1Path) {
    Write-Host "  -> forest_bird_1.wav (CC0 + ffmpeg)"
} else {
    $num = [int]($sampleRate * 0.45)
    $buf = New-Object float[] $num
    for ($i = 0; $i -lt $num; $i++) {
        $t = $i / $sampleRate
        $p = $t / 0.45
        $freq = 2200.0 + 900.0 * [Math]::Sin([Math]::PI * $p)
        $env = [Math]::Sin([Math]::PI * $p) * [Math]::Exp(-$t * 2.0)
        $buf[$i] = 0.45 * $env * [Math]::Sin($twoPi * $freq * $t)
    }
    Write-WavFile -FilePath $bird1Path -Samples $buf
    Write-Host "  -> forest_bird_1.wav (deterministic synth)"
}

$bird2Path = Join-Path $targetDir "forest_bird_2.wav"
if (Invoke-Cc0AssetDownload -FileName "forest_bird_2.wav" -OutputPath $bird2Path) {
    Write-Host "  -> forest_bird_2.wav (CC0 + ffmpeg)"
} else {
    $num = [int]($sampleRate * 0.50)
    $buf = New-Object float[] $num
    for ($i = 0; $i -lt $num; $i++) {
        $t = $i / $sampleRate
        $sub = ($t % 0.25) / 0.25
        $freq = 2700.0 - 700.0 * $sub + 900.0 * ($sub * $sub)
        $env = [Math]::Sin([Math]::PI * $sub)
        $buf[$i] = 0.42 * $env * [Math]::Sin($twoPi * $freq * $t)
    }
    Write-WavFile -FilePath $bird2Path -Samples $buf
    Write-Host "  -> forest_bird_2.wav (deterministic synth)"
}

$windPath = Join-Path $targetDir "forest_wind_grain.wav"
if (Invoke-Cc0AssetDownload -FileName "forest_wind_grain.wav" -OutputPath $windPath) {
    Write-Host "  -> forest_wind_grain.wav (CC0 + ffmpeg)"
} else {
    $rng = [System.Random]::new(503)
    $num = [int]($sampleRate * 0.80)
    $buf = New-Object float[] $num
    $lp = 0.0
    for ($i = 0; $i -lt $num; $i++) {
        $t = $i / $sampleRate
        $env = [Math]::Sin([Math]::PI * ($t / 0.80))
        $white = $rng.NextDouble() * 2.0 - 1.0
        $lp = 0.92 * $lp + 0.08 * $white
        $buf[$i] = 0.55 * $env * $lp
    }
    Write-WavFile -FilePath $windPath -Samples $buf
    Write-Host "  -> forest_wind_grain.wav (deterministic synth)"
}

$firePath = Join-Path $targetDir "forest_fire_crackle.wav"
if (Invoke-Cc0AssetDownload -FileName "forest_fire_crackle.wav" -OutputPath $firePath) {
    Write-Host "  -> forest_fire_crackle.wav (CC0 + ffmpeg)"
} else {
    $rng = [System.Random]::new(504)
    $num = [int]($sampleRate * 0.90)
    $buf = New-Object float[] $num
    for ($i = 0; $i -lt $num; $i++) {
        $t = $i / $sampleRate
        $env = [Math]::Sin([Math]::PI * ($t / 0.90))
        $pop = if ($rng.NextDouble() -lt 0.012) { ($rng.NextDouble() * 2.0 - 1.0) * 0.65 } else { ($rng.NextDouble() * 2.0 - 1.0) * 0.06 }
        $buf[$i] = $pop * $env
    }
    Write-WavFile -FilePath $firePath -Samples $buf
    Write-Host "  -> forest_fire_crackle.wav (deterministic synth)"
}

# 13. UI Click
$clickPath = Join-Path $targetDir "ui_click.wav"
if (Invoke-Cc0AssetDownload -FileName "ui_click.wav" -OutputPath $clickPath) {
    Write-Host "  -> ui_click.wav (CC0 + ffmpeg)"
} else {
    $numClick = [int]($sampleRate * 0.08)
    $clickBuf = New-Object float[] $numClick
    for ($i = 0; $i -lt $numClick; $i++) {
        $t = $i / $sampleRate
        $clickBuf[$i] = 0.55 * [Math]::Exp(-$t * 58.0) * [Math]::Sin($twoPi * 1350.0 * $t)
    }
    Write-WavFile -FilePath $clickPath -Samples $clickBuf
    Write-Host "  -> ui_click.wav (deterministic synth)"
}

# 14-16. Voiceover Dialogues (Neural pt-BR via edge-tts + ffmpeg, fallback to SAPI TTS, fallback to formant synth)
$voLines = @(
    @{ File = "vo_welcome.wav"; Voice = "pt-BR-AntonioNeural"; Text = "Bem-vindo ao Audio Playground tridimensional do OpenAudio Studio."; BaseHz = 135.0; Seed = 601 },
    @{ File = "vo_spatial.wav"; Voice = "pt-BR-FranciscaNeural"; Text = "Esta$([char]0x00E7)$([char]0x00E3)o espacial tridimensional e cones direcionais ativos."; BaseHz = 150.0; Seed = 602 },
    @{ File = "vo_ducking.wav"; Voice = "pt-BR-AntonioNeural"; Text = "Banco de voz carregado sob demanda com ducking aplicado na m$([char]0x00FA)sica."; BaseHz = 165.0; Seed = 603 }
)

foreach ($vo in $voLines) {
    $voPath = Join-Path $targetDir $vo.File
    if (Try-SynthesizeNeuralVoiceover -Text $vo.Text -Voice $vo.Voice -OutputPath $voPath) {
        Write-Host "  -> $($vo.File) (Neural TTS pt-BR + ffmpeg)"
    } elseif (-not $SyntheticOnly -and (Try-SynthesizeVoiceover -Text $vo.Text -OutputPath $voPath)) {
        Write-Host "  -> $($vo.File) (SAPI TTS + ffmpeg)"
    } else {
        $num = [int]($sampleRate * 1.6)
        $buf = New-Object float[] $num
        for ($i = 0; $i -lt $num; $i++) {
            $t = $i / $sampleRate
            $syllable = [Math]::Max(0.0, [Math]::Sin($twoPi * 3.5 * $t))
            $f0 = $vo.BaseHz + 18.0 * [Math]::Sin($twoPi * 1.5 * $t)
            $formant1 = [Math]::Sin($twoPi * ($f0 * 4.0) * $t)
            $formant2 = 0.5 * [Math]::Sin($twoPi * ($f0 * 9.0) * $t)
            $carrier = 0.45 * [Math]::Sin($twoPi * $f0 * $t) + 0.30 * $formant1 + 0.15 * $formant2
            $env = [Math]::Min(1.0, $t / 0.02) * [Math]::Min(1.0, (1.6 - $t) / 0.05)
            $buf[$i] = $carrier * (0.25 + 0.75 * $syllable) * $env * 0.6
        }
        Write-WavFile -FilePath $voPath -Samples $buf
        Write-Host "  -> $($vo.File) (deterministic formant fallback)"
    }
}

# Cleanup temp directory
if (Test-Path -LiteralPath $tempDir) {
    Remove-Item -LiteralPath $tempDir -Recurse -Force -ErrorAction SilentlyContinue
}

# Generate example_project/playground_project.json
$playgroundProject = [ordered]@{
    version = 1
    name = "AudioPlaygroundProject"
    buses = @(
        [ordered]@{
            path = "bus:/sfx"
            volume = 1.0
            effects = @(
                [ordered]@{
                    type = "reverb"
                    position = 0
                    params = @(0.12, 0.88, 0.55)
                    wet = 0.35
                    dry = 0.88
                }
            )
        },
        [ordered]@{
            path = "bus:/music"
            volume = 1.0
            effects = @(
                [ordered]@{
                    type = "compressor"
                    position = 0
                    params = @(-24.0, 6.0, 15.0, 650.0, 0.0)
                    wet = 1.0
                    dry = 0.0
                    sidechain_bus = "bus:/vo"
                },
                [ordered]@{
                    type = "lowpass"
                    position = 0
                    params = @(20000.0, 0.707)
                    wet = 1.0
                    dry = 0.0
                }
            )
        },
        [ordered]@{
            path = "bus:/ambience"
            volume = 1.0
            effects = @(
                [ordered]@{
                    type = "lowpass"
                    position = 0
                    params = @(20000.0, 0.707)
                    wet = 1.0
                    dry = 0.0
                }
            )
        },
        [ordered]@{
            path = "bus:/vo"
            volume = 1.0
            effects = @(
                [ordered]@{
                    type = "reverb"
                    position = 0
                    params = @(0.10, 0.90, 0.45)
                    wet = 0.25
                    dry = 0.95
                }
            )
        },
        [ordered]@{ path = "bus:/ui"; volume = 1.0; effects = @() }
    )
    global_parameters = @(
        [ordered]@{
            name = "SurfaceType"
            kind = "labeled"
            labels = @("Wood", "Concrete", "Grass")
            default_value = 0
        },
        [ordered]@{
            name = "CombatIntensity"
            kind = "continuous"
            min = 0.0
            max = 1.0
            default_value = 0.0
            seek_speed = 0.8
            seek_speed_down = 0.6
        }
    )
    vcas = @(
        [ordered]@{
            path = "vca:/playground_master"
            buses = @("bus:/sfx", "bus:/music", "bus:/ambience", "bus:/vo", "bus:/ui")
        },
        [ordered]@{
            path = "vca:/playground_music"
            buses = @("bus:/music")
        },
        [ordered]@{
            path = "vca:/playground_sfx"
            buses = @("bus:/sfx", "bus:/ambience", "bus:/ui")
        }
    )
    samples = @(
        [ordered]@{ id = "sample_step_wood_1"; file = "assets/playground/step_wood_1.wav" },
        [ordered]@{ id = "sample_step_wood_2"; file = "assets/playground/step_wood_2.wav" },
        [ordered]@{ id = "sample_step_wood_3"; file = "assets/playground/step_wood_3.wav" },
        [ordered]@{ id = "sample_step_wood_4"; file = "assets/playground/step_wood_4.wav" },
        [ordered]@{ id = "sample_step_concrete_1"; file = "assets/playground/step_concrete_1.wav" },
        [ordered]@{ id = "sample_step_concrete_2"; file = "assets/playground/step_concrete_2.wav" },
        [ordered]@{ id = "sample_step_concrete_3"; file = "assets/playground/step_concrete_3.wav" },
        [ordered]@{ id = "sample_step_concrete_4"; file = "assets/playground/step_concrete_4.wav" },
        [ordered]@{ id = "sample_step_grass_1"; file = "assets/playground/step_grass_1.wav" },
        [ordered]@{ id = "sample_step_grass_2"; file = "assets/playground/step_grass_2.wav" },
        [ordered]@{ id = "sample_step_grass_3"; file = "assets/playground/step_grass_3.wav" },
        [ordered]@{ id = "sample_step_grass_4"; file = "assets/playground/step_grass_4.wav" },
        [ordered]@{ id = "sample_emitter_3d"; file = "assets/playground/emitter_3d_loop.wav" },
        [ordered]@{ id = "sample_music_explore"; file = "assets/playground/music_explore.wav"; stream = $true },
        [ordered]@{ id = "sample_music_combat"; file = "assets/playground/music_combat.wav"; stream = $true },
        [ordered]@{ id = "sample_music_tension"; file = "assets/playground/music_tension_layer.wav"; stream = $true },
        [ordered]@{ id = "sample_forest_bird_1"; file = "assets/playground/forest_bird_1.wav" },
        [ordered]@{ id = "sample_forest_bird_2"; file = "assets/playground/forest_bird_2.wav" },
        [ordered]@{ id = "sample_forest_wind"; file = "assets/playground/forest_wind_grain.wav" },
        [ordered]@{ id = "sample_forest_fire"; file = "assets/playground/forest_fire_crackle.wav" },
        [ordered]@{ id = "sample_ui_click"; file = "assets/playground/ui_click.wav" }
    )
    events = @(
        [ordered]@{
            path = "event:/playground/footstep"
            kind = "event"
            bus_path = "bus:/sfx"
            volume_db = 1.0
            max_instances = 6
            steal_mode = "oldest"
            cooldown_ms = 40
            tracks = @(
                [ordered]@{
                    volume_db = 0.0
                    instruments = @(
                        [ordered]@{
                            kind = "multi"
                            playlist_mode = "random_no_repeat"
                            playlist = @(
                                [ordered]@{ sample_id = "sample_step_wood_1"; weight = 0.25 },
                                [ordered]@{ sample_id = "sample_step_wood_2"; weight = 0.25 },
                                [ordered]@{ sample_id = "sample_step_wood_3"; weight = 0.25 },
                                [ordered]@{ sample_id = "sample_step_wood_4"; weight = 0.25 }
                            )
                            offset_ms = 0.0
                            length_ms = 280.0
                            loop = $false
                            volume_db_random_range = 2.0
                            pitch_semitones_random_range = 1.8
                            condition = [ordered]@{
                                probability_pct = 100.0
                                terms = @(
                                    [ordered]@{
                                        param = [ordered]@{ scope = "global"; name = "SurfaceType" }
                                        min = 0.0
                                        max = 0.4
                                    }
                                )
                            }
                        },
                        [ordered]@{
                            kind = "multi"
                            playlist_mode = "random_no_repeat"
                            playlist = @(
                                [ordered]@{ sample_id = "sample_step_concrete_1"; weight = 0.25 },
                                [ordered]@{ sample_id = "sample_step_concrete_2"; weight = 0.25 },
                                [ordered]@{ sample_id = "sample_step_concrete_3"; weight = 0.25 },
                                [ordered]@{ sample_id = "sample_step_concrete_4"; weight = 0.25 }
                            )
                            offset_ms = 0.0
                            length_ms = 280.0
                            loop = $false
                            volume_db_random_range = 2.0
                            pitch_semitones_random_range = 1.8
                            condition = [ordered]@{
                                probability_pct = 100.0
                                terms = @(
                                    [ordered]@{
                                        param = [ordered]@{ scope = "global"; name = "SurfaceType" }
                                        min = 0.6
                                        max = 1.4
                                    }
                                )
                            }
                        },
                        [ordered]@{
                            kind = "multi"
                            playlist_mode = "random_no_repeat"
                            playlist = @(
                                [ordered]@{ sample_id = "sample_step_grass_1"; weight = 0.25 },
                                [ordered]@{ sample_id = "sample_step_grass_2"; weight = 0.25 },
                                [ordered]@{ sample_id = "sample_step_grass_3"; weight = 0.25 },
                                [ordered]@{ sample_id = "sample_step_grass_4"; weight = 0.25 }
                            )
                            offset_ms = 0.0
                            length_ms = 280.0
                            loop = $false
                            volume_db_random_range = 2.0
                            pitch_semitones_random_range = 1.8
                            condition = [ordered]@{
                                probability_pct = 100.0
                                terms = @(
                                    [ordered]@{
                                        param = [ordered]@{ scope = "global"; name = "SurfaceType" }
                                        min = 1.6
                                        max = 2.0
                                    }
                                )
                            }
                        }
                    )
                }
            )
        },
        [ordered]@{
            path = "event:/playground/emitter_3d"
            kind = "event"
            bus_path = "bus:/sfx"
            volume_db = 1.5
            stop_fadeout_ms = 350.0
            parameters = @(
                [ordered]@{
                    name = "EventConeAngle"
                    kind = "continuous"
                    min = 0.0
                    max = 180.0
                    default_value = 0.0
                }
            )
            automations = @(
                [ordered]@{
                    target = "volume"
                    is_timeline = $false
                    param = [ordered]@{ scope = "local"; name = "EventConeAngle" }
                    curve = [ordered]@{
                        points = @(
                            [ordered]@{ x = 0.0; y = 1.0; shape = "linear" },
                            [ordered]@{ x = 45.0; y = -2.0; shape = "s_curve" },
                            [ordered]@{ x = 115.0; y = -16.0; shape = "s_curve" },
                            [ordered]@{ x = 180.0; y = -24.0; shape = "linear" }
                        )
                    }
                }
            )
            tracks = @(
                [ordered]@{
                    volume_db = 0.0
                    instruments = @(
                        [ordered]@{
                            kind = "single"
                            sample_id = "sample_emitter_3d"
                            offset_ms = 0.0
                            length_ms = 16000.0
                            fade_in_ms = 30.0
                            fade_out_ms = 200.0
                            loop = $true
                        }
                    )
                }
            )
            logic_items = @(
                [ordered]@{
                    kind = "loop_region"
                    start_ms = 0.0
                    length_ms = 16000.0
                    iterations = -1
                    until_key_off = $false
                }
            )
        },
        [ordered]@{
            path = "event:/playground/adaptive_music"
            kind = "event"
            bus_path = "bus:/music"
            volume_db = -3.0
            stop_fadeout_ms = 900.0
            tracks = @(
                [ordered]@{
                    volume_db = 0.0
                    instruments = @(
                        [ordered]@{
                            kind = "single"
                            sample_id = "sample_music_explore"
                            offset_ms = 0.0
                            length_ms = 16000.0
                            fade_in_ms = 600.0
                            fade_out_ms = 800.0
                            loop = $true
                            modulators = @(
                                [ordered]@{
                                    kind = "ahdsr"
                                    attack_ms = 450.0
                                    hold_ms = 0.0
                                    decay_ms = 0.0
                                    sustain = 1.0
                                    release_ms = 850.0
                                    release_on_key_off = $true
                                }
                            )
                        },
                        [ordered]@{
                            kind = "single"
                            sample_id = "sample_music_combat"
                            offset_ms = 18000.0
                            length_ms = 16000.0
                            fade_in_ms = 450.0
                            fade_out_ms = 900.0
                            loop = $true
                            modulators = @(
                                [ordered]@{
                                    kind = "ahdsr"
                                    attack_ms = 350.0
                                    hold_ms = 0.0
                                    decay_ms = 0.0
                                    sustain = 1.0
                                    release_ms = 900.0
                                    release_on_key_off = $true
                                }
                            )
                        }
                    )
                },
                [ordered]@{
                    volume_db = -4.0
                    automations = @(
                        [ordered]@{
                            target = "volume"
                            is_timeline = $false
                            param = [ordered]@{ scope = "global"; name = "CombatIntensity" }
                            curve = [ordered]@{
                                points = @(
                                    [ordered]@{ x = 0.0; y = -28.0; shape = "ease_in" },
                                    [ordered]@{ x = 0.5; y = -10.0; shape = "s_curve" },
                                    [ordered]@{ x = 1.0; y = -2.0; shape = "ease_out" }
                                )
                            }
                        }
                    )
                    instruments = @(
                        [ordered]@{
                            kind = "single"
                            sample_id = "sample_music_tension"
                            offset_ms = 18000.0
                            length_ms = 16000.0
                            fade_in_ms = 500.0
                            fade_out_ms = 1000.0
                            loop = $true
                        }
                    )
                }
            )
            logic_items = @(
                [ordered]@{
                    kind = "tempo"
                    position_ms = 0.0
                    bpm = 120.0
                    ts_upper = 4
                    ts_lower = 4
                },
                [ordered]@{
                    kind = "marker"
                    position_ms = 0.0
                    name = "Exploration"
                },
                [ordered]@{
                    kind = "loop_region"
                    start_ms = 0.0
                    length_ms = 16000.0
                    iterations = -1
                    until_key_off = $false
                },
                [ordered]@{
                    kind = "transition_region"
                    start_ms = 0.0
                    length_ms = 16000.0
                    target_marker = "Combat"
                    quantization = "immediate"
                    crossfade_ms = 1400.0
                    condition = [ordered]@{
                        probability_pct = 100.0
                        terms = @(
                            [ordered]@{
                                param = [ordered]@{ scope = "global"; name = "CombatIntensity" }
                                min = 0.5
                                max = 1.0
                            }
                        )
                    }
                },
                [ordered]@{
                    kind = "marker"
                    position_ms = 18000.0
                    name = "Combat"
                },
                [ordered]@{
                    kind = "loop_region"
                    start_ms = 18000.0
                    length_ms = 16000.0
                    iterations = -1
                    until_key_off = $false
                },
                [ordered]@{
                    kind = "transition_region"
                    start_ms = 18000.0
                    length_ms = 16000.0
                    target_marker = "Exploration"
                    quantization = "immediate"
                    crossfade_ms = 1600.0
                    condition = [ordered]@{
                        probability_pct = 100.0
                        terms = @(
                            [ordered]@{
                                param = [ordered]@{ scope = "global"; name = "CombatIntensity" }
                                min = 0.0
                                max = 0.49
                            }
                        )
                    }
                }
            )
        },
        [ordered]@{
            path = "event:/playground/forest_scatterer"
            kind = "event"
            bus_path = "bus:/ambience"
            volume_db = -1.5
            stop_fadeout_ms = 800.0
            tracks = @(
                [ordered]@{
                    volume_db = 0.0
                    instruments = @(
                        [ordered]@{
                            kind = "scatterer"
                            offset_ms = 0.0
                            length_ms = 6000.0
                            fade_in_ms = 150.0
                            fade_out_ms = 350.0
                            async = $true
                            playlist_mode = "random_no_repeat"
                            playlist = @(
                                [ordered]@{ sample_id = "sample_forest_bird_1"; weight = 0.35 },
                                [ordered]@{ sample_id = "sample_forest_bird_2"; weight = 0.25 },
                                [ordered]@{ sample_id = "sample_forest_wind"; weight = 0.22 },
                                [ordered]@{ sample_id = "sample_forest_fire"; weight = 0.18 }
                            )
                            spawn_interval_min_ms = 220.0
                            spawn_interval_max_ms = 620.0
                            polyphony = 4
                            total_sounds = 0
                            min_scatter_distance = 1.5
                            max_scatter_distance = 9.0
                            volume_db_random_range = 2.5
                            pitch_semitones_random_range = 1.8
                        }
                    )
                }
            )
            logic_items = @(
                [ordered]@{
                    kind = "loop_region"
                    start_ms = 0.0
                    length_ms = 6000.0
                    iterations = -1
                    until_key_off = $false
                }
            )
        },
        [ordered]@{
            path = "event:/playground/ui_click"
            kind = "event"
            bus_path = "bus:/ui"
            volume_db = -1.0
            tracks = @(
                [ordered]@{
                    volume_db = 0.0
                    instruments = @(
                        [ordered]@{
                            kind = "single"
                            sample_id = "sample_ui_click"
                            offset_ms = 0.0
                            length_ms = 80.0
                            loop = $false
                        }
                    )
                }
            )
        },
        [ordered]@{
            path = "snapshot:/playground/cave_reverb"
            kind = "snapshot"
            priority = 180
            snapshot_mode = "override"
            fade_in_ms = 450
            stop_fadeout_ms = 750
            snapshot_targets = @(
                [ordered]@{ bus_path = "bus:/music"; value = -6.0; param_id = -1 },
                [ordered]@{ bus_path = "bus:/music"; position = 0; effect_index = 1; effect_type = "lowpass"; param_id = 0; value = 950.0 },
                [ordered]@{ bus_path = "bus:/sfx"; value = 2.0; param_id = -1 },
                [ordered]@{ bus_path = "bus:/sfx"; position = 0; effect_index = 0; effect_type = "reverb"; param_id = 0; value = 0.94 },
                [ordered]@{ bus_path = "bus:/sfx"; position = 0; effect_index = 0; effect_type = "reverb"; param_id = 1; value = 0.18 },
                [ordered]@{ bus_path = "bus:/vo"; position = 0; effect_index = 0; effect_type = "reverb"; param_id = 0; value = 0.88 },
                [ordered]@{ bus_path = "bus:/vo"; position = 0; effect_index = 0; effect_type = "reverb"; param_id = 1; value = 0.22 },
                [ordered]@{ bus_path = "bus:/ambience"; value = -8.0; param_id = -1 },
                [ordered]@{ bus_path = "bus:/ambience"; position = 0; effect_index = 0; effect_type = "lowpass"; param_id = 0; value = 750.0 }
            )
        },
        [ordered]@{
            path = "snapshot:/playground/vo_ducking"
            kind = "snapshot"
            priority = 220
            snapshot_mode = "override"
            fade_in_ms = 160
            stop_fadeout_ms = 900
            snapshot_targets = @(
                [ordered]@{ bus_path = "bus:/music"; value = -14.0; param_id = -1 },
                [ordered]@{ bus_path = "bus:/ambience"; value = -8.0; param_id = -1 }
            )
        }
    )
    effect_automations = @()
    mixer = [ordered]@{
        master = [ordered]@{
            volume = 1.0
            mute = $false
            effects = @(
                [ordered]@{
                    type = "limiter"
                    position = 0
                    params = @(-1.0, 80.0, 0.0)
                    wet = 1.0
                    dry = 0.0
                }
            )
        }
    }
}

# Generate example_project/playground_vo_project.json
$playgroundVoProject = [ordered]@{
    version = 1
    name = "AudioPlaygroundVoiceProject"
    buses = @(
        [ordered]@{
            path = "bus:/vo"
            volume = 1.0
            effects = @(
                [ordered]@{
                    type = "reverb"
                    position = 0
                    params = @(0.10, 0.90, 0.45)
                    wet = 0.25
                    dry = 0.95
                }
            )
        },
        [ordered]@{
            path = "bus:/music"
            volume = 1.0
            effects = @(
                [ordered]@{
                    type = "compressor"
                    position = 0
                    params = @(-24.0, 6.0, 15.0, 650.0, 0.0)
                    wet = 1.0
                    dry = 0.0
                    sidechain_bus = "bus:/vo"
                },
                [ordered]@{
                    type = "lowpass"
                    position = 0
                    params = @(20000.0, 0.707)
                    wet = 1.0
                    dry = 0.0
                }
            )
        }
    )
    global_parameters = @()
    vcas = @()
    samples = @(
        [ordered]@{ id = "sample_vo_welcome"; file = "assets/playground/vo_welcome.wav" },
        [ordered]@{ id = "sample_vo_spatial"; file = "assets/playground/vo_spatial.wav" },
        [ordered]@{ id = "sample_vo_ducking"; file = "assets/playground/vo_ducking.wav" }
    )
    audio_table = @(
        [ordered]@{ key = "vo_welcome"; sample_id = "sample_vo_welcome" },
        [ordered]@{ key = "vo_spatial_test"; sample_id = "sample_vo_spatial" },
        [ordered]@{ key = "vo_ducking_active"; sample_id = "sample_vo_ducking" }
    )
    events = @(
        [ordered]@{
            path = "event:/playground/dialogue_vo"
            kind = "event"
            bus_path = "bus:/vo"
            volume_db = 0.0
            parameters = @(
                [ordered]@{
                    name = "DialogueLine"
                    kind = "labeled"
                    labels = @("vo_welcome", "vo_spatial_test", "vo_ducking_active")
                    default_value = 0
                }
            )
            tracks = @(
                [ordered]@{
                    volume_db = 0.0
                    instruments = @(
                        [ordered]@{
                            kind = "programmer"
                            programmer_key = "vo_welcome"
                            offset_ms = 0.0
                            length_ms = 5000.0
                            condition = [ordered]@{
                                probability_pct = 100.0
                                terms = @(
                                    [ordered]@{
                                        param = [ordered]@{ scope = "local"; name = "DialogueLine" }
                                        min = 0.0
                                        max = 0.4
                                    }
                                )
                            }
                        },
                        [ordered]@{
                            kind = "programmer"
                            programmer_key = "vo_spatial_test"
                            offset_ms = 0.0
                            length_ms = 5000.0
                            condition = [ordered]@{
                                probability_pct = 100.0
                                terms = @(
                                    [ordered]@{
                                        param = [ordered]@{ scope = "local"; name = "DialogueLine" }
                                        min = 0.6
                                        max = 1.4
                                    }
                                )
                            }
                        },
                        [ordered]@{
                            kind = "programmer"
                            programmer_key = "vo_ducking_active"
                            offset_ms = 0.0
                            length_ms = 5000.0
                            condition = [ordered]@{
                                probability_pct = 100.0
                                terms = @(
                                    [ordered]@{
                                        param = [ordered]@{ scope = "local"; name = "DialogueLine" }
                                        min = 1.6
                                        max = 2.0
                                    }
                                )
                            }
                        }
                    )
                }
            )
        }
    )
    effect_automations = @()
    mixer = [ordered]@{
        master = [ordered]@{
            volume = 1.0
            mute = $false
            effects = @(
                [ordered]@{
                    type = "limiter"
                    position = 0
                    params = @(-1.0, 80.0, 0.0)
                    wet = 1.0
                    dry = 0.0
                }
            )
        }
    }
}

$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$mainJsonPath = Join-Path $repoRoot "example_project\playground_project.json"
$voJsonPath = Join-Path $repoRoot "example_project\playground_vo_project.json"

[System.IO.File]::WriteAllText($mainJsonPath, ($playgroundProject | ConvertTo-Json -Depth 12), $utf8NoBom)
[System.IO.File]::WriteAllText($voJsonPath, ($playgroundVoProject | ConvertTo-Json -Depth 12), $utf8NoBom)
Write-Host "[Playground Assets] Generated JSON manifests in example_project/." -ForegroundColor Green

# Compile banks with oae_bankc.exe
$bankcCandidates = @(
    (Join-Path $repoRoot "build\Studio_Bundle_Release\oae_bankc.exe"),
    (Join-Path $repoRoot "build\Release\Release\oae_bankc.exe"),
    (Join-Path $repoRoot "build\Debug\Debug\oae_bankc.exe")
)
$bankcExe = $bankcCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $bankcExe) {
    throw "oae_bankc.exe not found. Build native or Studio Release first."
}

$outMainBank = Join-Path $repoRoot "example_project\playground.bank"
$outVoBank = Join-Path $repoRoot "example_project\playground_vo.bank"

Write-Host "[Playground Assets] Compiling playground.bank with $bankcExe..." -ForegroundColor Cyan
& $bankcExe $mainJsonPath -o $outMainBank
if ($LASTEXITCODE -ne 0) { throw "Failed to compile playground.bank (exit $LASTEXITCODE)." }

Write-Host "[Playground Assets] Compiling playground_vo.bank with $bankcExe..." -ForegroundColor Cyan
& $bankcExe $voJsonPath -o $outVoBank
if ($LASTEXITCODE -ne 0) { throw "Failed to compile playground_vo.bank (exit $LASTEXITCODE)." }

foreach ($b in @("build\Studio_Bundle_Release", "build\Studio_Bundle_Debug")) {
    $dest = Join-Path $repoRoot "$b\example_project"
    if (Test-Path -LiteralPath $dest) {
        Copy-Item (Join-Path $repoRoot "example_project\playground*.bank*") $dest -Force
    }
}

Copy-Item (Join-Path $repoRoot "example_project\playground*.bank*") $saDir -Force
Write-Host "[Playground Assets] Deployed playground.bank & playground_vo.bank (with catalogs) to $saDir" -ForegroundColor Green
