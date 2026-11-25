$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$exampleDir = Join-Path $repoRoot "example_project"
$assetsDir = Join-Path $exampleDir "assets"
$buildDir = Join-Path $exampleDir "build"

if (-not (Test-Path $assetsDir)) { New-Item -ItemType Directory -Path $assetsDir -Force | Out-Null }
if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir -Force | Out-Null }

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$exampleDir = Join-Path $repoRoot "example_project"
$assetsDir = Join-Path $exampleDir "assets"
$buildDir = Join-Path $exampleDir "build"

if (-not (Test-Path $assetsDir)) { New-Item -ItemType Directory -Path $assetsDir -Force | Out-Null }
if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir -Force | Out-Null }

function Write-Wav([string]$path, [float[]]$samples, [int]$sampleRate = 48000) {
    $numSamples = $samples.Length
    $numChannels = 1
    $bitsPerSample = 16
    $dataSize = $numSamples * 2
    $chunkSize = 36 + $dataSize

    $fs = [System.IO.File]::Create($path)
    $bw = New-Object System.IO.BinaryWriter($fs)

    $bw.Write([System.Text.Encoding]::ASCII.GetBytes('RIFF'))
    $bw.Write([uint32]$chunkSize)
    $bw.Write([System.Text.Encoding]::ASCII.GetBytes('WAVEfmt '))
    $bw.Write([uint32]16)
    $bw.Write([uint16]1) # PCM
    $bw.Write([uint16]$numChannels)
    $bw.Write([uint32]$sampleRate)
    $bw.Write([uint32]($sampleRate * 2))
    $bw.Write([uint16]2)
    $bw.Write([uint16]$bitsPerSample)
    $bw.Write([System.Text.Encoding]::ASCII.GetBytes('data'))
    $bw.Write([uint32]$dataSize)

    for ($i = 0; $i -lt $numSamples; $i++) {
        $val = [Math]::Max(-1.0, [Math]::Min(1.0, $samples[$i]))
        $sampleInt = [int16]($val * 32767.0)
        $bw.Write($sampleInt)
    }

    $bw.Close()
    $fs.Close()
}

# 1. Synthesize theme.wav (2.0s musical synth-bass loop with clear attack, decay, and rhythmic transients)
$sampleRate = 48000
$themeDuration = 2.0
$themeSamples = New-Object float[] ([int]($sampleRate * $themeDuration))
$notes = @(110.0, 130.81, 164.81, 220.0, 196.0, 164.81, 146.83, 164.81)
$stepDuration = 0.25
$stepSamples = [int]($sampleRate * $stepDuration)

for ($s = 0; $s -lt 8; $s++) {
    $freq = $notes[$s]
    $startIdx = $s * $stepSamples
    for ($i = 0; $i -lt $stepSamples; $i++) {
        $idx = $startIdx + $i
        $tStep = $i / $sampleRate
        if ($tStep -lt 0.20) {
            $att = [Math]::Min(1.0, $tStep / 0.015)
            $dec = [Math]::Exp(-6.0 * $tStep)
            $env = $att * $dec * 0.8
            $tGlobal = $idx / $sampleRate
            $osc = [Math]::Sin(2.0 * [Math]::PI * $freq * $tGlobal) + 0.35 * [Math]::Sin(4.0 * [Math]::PI * $freq * $tGlobal) + 0.15 * [Math]::Sin(6.0 * [Math]::PI * $freq * $tGlobal)
            $themeSamples[$idx] = [float]($env * $osc)
        } else {
            $themeSamples[$idx] = [float]0.0
        }
    }
}
Write-Wav (Join-Path $assetsDir "theme.wav") $themeSamples $sampleRate

# 2. Synthesize coin.wav (0.3s retro game double-chime with crisp transients and exponential ringout)
$coinDuration = 0.3
$coinSamples = New-Object float[] ([int]($sampleRate * $coinDuration))

for ($i = 0; $i -lt $coinSamples.Length; $i++) {
    $t = $i / $sampleRate
    if ($t -lt 0.08) {
        $freq = 987.77
        $att = [Math]::Min(1.0, $t / 0.005)
        $dec = [Math]::Exp(-15.0 * $t)
        $env = $att * $dec * 0.75
        $coinSamples[$i] = [float]($env * [Math]::Sin(2.0 * [Math]::PI * $freq * $t))
    } else {
        $freq = 1318.51
        $tNote = $t - 0.08
        $att = [Math]::Min(1.0, $tNote / 0.005)
        $dec = [Math]::Exp(-10.0 * $tNote)
        $env = $att * $dec * 0.85
        $coinSamples[$i] = [float]($env * [Math]::Sin(2.0 * [Math]::PI * $freq * $tNote))
    }
}
Write-Wav (Join-Path $assetsDir "coin.wav") $coinSamples $sampleRate

Write-Host "[ExampleProject] Audio assets generated successfully with rich musical dynamics."

# 3. Recompile master.bank if oae_bankc executable is found
$bankcCandidates = @(
    (Join-Path $repoRoot "build/Release/oae_bankc.exe"),
    (Join-Path $repoRoot "build/Debug/oae_bankc.exe"),
    (Join-Path $repoRoot "build/Release/Release/oae_bankc.exe"),
    (Join-Path $repoRoot "build/Debug/Debug/oae_bankc.exe"),
    (Join-Path $repoRoot "build/Studio_Bundle_Release/oae_bankc.exe")
)
$bankc = $null
foreach ($c in $bankcCandidates) {
    if (Test-Path -LiteralPath $c -PathType Leaf) {
        $bankc = $c
        break
    }
}

if ($bankc) {
    Write-Host "[ExampleProject] Compiling master.bank via $bankc..."
    $projJson = Join-Path $exampleDir "project.json"
    $outBank = Join-Path $buildDir "master.bank"
    $catalogJson = Join-Path $buildDir "master.bank.catalog.json"
    $p = Start-Process -FilePath $bankc -ArgumentList "`"$projJson`" -o `"$outBank`"" -NoNewWindow -Wait -PassThru
    if ($p.ExitCode -eq 0) {
        Write-Host "[ExampleProject] master.bank compiled successfully."
    } else {
        Write-Warning "[ExampleProject] oae_bankc exited with code $($p.ExitCode)"
    }
}
