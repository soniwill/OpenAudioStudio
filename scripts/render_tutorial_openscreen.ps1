<#
.SYNOPSIS
    Renderiza localmente tutoriais do OpenAudio Studio com OpenScreen 2.3.0, TTS pt-BR, legendas SRT e masterizacao -14 LUFS.

.DESCRIPTION
    Orquestra a preparacao do OpenAudio Studio via MCP, gravacao de audio WASAPI real com
    OpenScreen CLI (e captura direta via studio_ui_screenshot quando em desktop isolado exebox),
    estilizacao e zoom em arquivo editavel .openscreen, exportacao a 60 fps pelo compositor do
    OpenScreen, mixagem com sidechain ducking sobre narração neural edge-tts e masterizacao
    final via scripts/finalize_tutorial_video.ps1.
#>
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = "Low")]
param(
    [string]$Episode = "03A",

    [string]$OutputDir = "build/tutorial_videos",

    [string]$Voice = "pt-BR-AntonioNeural",

    [ValidateSet("Auto", "OpenScreenLive", "StudioMcp")]
    [string]$CaptureMode = "Auto",

    [string]$OpenscreenPath = "",

    [string]$StudioExePath = "build/Studio_Bundle_Release/OpenAudio Studio.exe",

    [bool]$BurnSubtitles = $true,

    [bool]$EnableZoom = $true,

    [int]$Padding = 14,

    [int]$BorderRadius = 16,

    [switch]$KeepWork
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptDir ".."))
$InvCulture = [System.Globalization.CultureInfo]::InvariantCulture
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Unescape-Text {
    param([string]$Text)
    if ([string]::IsNullOrEmpty($Text)) { return "" }
    return [System.Text.RegularExpressions.Regex]::Unescape($Text)
}

function Resolve-ToolPath {
    param(
        [string]$Candidate,
        [string[]]$Fallbacks,
        [string]$FriendlyName
    )
    if (-not [string]::IsNullOrWhiteSpace($Candidate)) {
        if (Test-Path -LiteralPath $Candidate) {
            return [System.IO.Path]::GetFullPath($Candidate)
        }
        $cmd = Get-Command $Candidate -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    foreach ($fb in $Fallbacks) {
        if ([string]::IsNullOrWhiteSpace($fb)) { continue }
        if (Test-Path -LiteralPath $fb) {
            return [System.IO.Path]::GetFullPath($fb)
        }
        $cmd = Get-Command $fb -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    throw "Ferramenta '$FriendlyName' nao encontrada. Verifique a instalacao ou informe o caminho explicito."
}

function Ensure-Win32Interop {
    if (-not ("OasRenderWin32" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class OasRenderWin32 {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT {
        public int X;
        public int Y;
    }

    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);

    [DllImport("user32.dll")]
    public static extern IntPtr GetThreadDesktop(uint dwThreadId);

    [DllImport("kernel32.dll")]
    public static extern uint GetCurrentThreadId();

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern bool GetUserObjectInformation(IntPtr hObj, int nIndex, StringBuilder pvInfo, uint nLength, out uint lpnLengthNeeded);

    public static string GetCurrentDesktopName() {
        IntPtr desk = GetThreadDesktop(GetCurrentThreadId());
        StringBuilder name = new StringBuilder(256);
        uint needed = 0;
        if (GetUserObjectInformation(desk, 2, name, 512, out needed)) {
            return name.ToString();
        }
        return "";
    }

    public static IntPtr FindMainWindow(int pid, string titleContains) {
        IntPtr found = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hWnd, IntPtr lParam) {
            uint winPid = 0;
            GetWindowThreadProcessId(hWnd, out winPid);
            if ((int)winPid != pid) return true;
            if (!IsWindowVisible(hWnd)) return true;
            StringBuilder sb = new StringBuilder(512);
            GetWindowText(hWnd, sb, sb.Capacity);
            string title = sb.ToString();
            if (string.IsNullOrEmpty(titleContains) || title.IndexOf(titleContains, StringComparison.OrdinalIgnoreCase) >= 0) {
                found = hWnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
"@
    }
}

function Format-SrtTime {
    param([int]$Ms)
    if ($Ms -lt 0) { $Ms = 0 }
    $ts = [TimeSpan]::FromMilliseconds($Ms)
    return ("{0:00}:{1:00}:{2:00},{3:000}" -f [int][Math]::Floor($ts.TotalHours), $ts.Minutes, $ts.Seconds, $ts.Milliseconds)
}

function Split-CaptionLines {
    param(
        [string]$Text,
        [int]$MaxLineChars = 48
    )
    $words = ($Text.Trim() -replace '\s+', ' ').Split(' ')
    if ($words.Length -eq 0) { return "" }
    $lines = New-Object System.Collections.Generic.List[string]
    $current = ""
    foreach ($w in $words) {
        if ([string]::IsNullOrEmpty($current)) {
            $current = $w
        } elseif (($current.Length + 1 + $w.Length) -le $MaxLineChars) {
            $current = "$current $w"
        } else {
            $lines.Add($current)
            $current = $w
        }
    }
    if (-not [string]::IsNullOrEmpty($current)) {
        $lines.Add($current)
    }
    if ($lines.Count -le 2) {
        return ($lines -join "`r`n")
    }
    # Rebalanceia para no maximo 2 linhas equilibradas
    $full = ($words -join ' ')
    $mid = [int][Math]::Floor($full.Length / 2)
    $bestIdx = -1
    $bestDist = 99999
    for ($i = 0; $i -lt $full.Length; $i++) {
        if ($full[$i] -eq ' ') {
            $d = [Math]::Abs($i - $mid)
            if ($d -lt $bestDist) {
                $bestDist = $d
                $bestIdx = $i
            }
        }
    }
    if ($bestIdx -gt 0) {
        return ($full.Substring(0, $bestIdx).Trim() + "`r`n" + $full.Substring($bestIdx + 1).Trim())
    }
    return ($lines -join "`r`n")
}

function Split-ToTwoSubtitleParts {
    param(
        [string]$Text,
        [int]$StartMs,
        [int]$EndMs
    )
    $clean = ($Text.Trim() -replace '\s+', ' ')
    $dur = $EndMs - $StartMs
    if ($clean.Length -le 88 -or $dur -lt 3200) {
        return @(@{ startMs = $StartMs; endMs = $EndMs; text = (Split-CaptionLines -Text $clean -MaxLineChars 46) })
    }
    $words = $clean.Split(' ')
    $targetChars = [Math]::Floor($clean.Length / 2)
    $acc = 0
    $splitIdx = [Math]::Floor($words.Length / 2)
    $bestDiff = 99999
    for ($i = 1; $i -lt $words.Length; $i++) {
        $acc += $words[$i - 1].Length + 1
        $punctBonus = if ($words[$i - 1].EndsWith(',') -or $words[$i - 1].EndsWith(';')) { -12 } else { 0 }
        $diff = [Math]::Abs($acc - $targetChars) + $punctBonus
        if ($diff -lt $bestDiff) {
            $bestDiff = $diff
            $splitIdx = $i
        }
    }
    $part1 = ($words[0..($splitIdx - 1)] -join ' ').Trim()
    $part2 = ($words[$splitIdx..($words.Length - 1)] -join ' ').Trim()
    $ratio = [double]$part1.Length / [double]([Math]::Max(1, ($part1.Length + $part2.Length)))
    $midMs = $StartMs + [int][Math]::Round($dur * $ratio)
    return @(
        @{ startMs = $StartMs; endMs = $midMs; text = (Split-CaptionLines -Text $part1 -MaxLineChars 46) },
        @{ startMs = $midMs; endMs = $EndMs; text = (Split-CaptionLines -Text $part2 -MaxLineChars 46) }
    )
}

function Invoke-NativeCapture {
    param(
        [string]$Exe,
        [string[]]$CmdArgs
    )
    $prev = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $out = & $Exe @CmdArgs 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
    return @{ ExitCode = $code; Output = @($out | ForEach-Object { $_.ToString() }) }
}

function Invoke-OpenScreenCli {
    param(
        [string]$NodeExe,
        [string]$OpenscreenExe,
        [string[]]$CliArgs
    )
    $runnerJs = @'
const { spawn } = require('child_process');
const exe = process.argv[1];
const args = process.argv.slice(2);
const child = spawn(exe, args, { stdio: ['ignore', 'pipe', 'pipe'] });
child.stdout.on('data', (d) => process.stdout.write(d));
child.stderr.on('data', (d) => process.stderr.write(d));
child.on('error', (err) => { console.error(err.message); process.exit(1); });
child.on('close', (code) => process.exit(code === null ? 1 : code));
'@
    $nodeArgs = @("-e", $runnerJs, $OpenscreenExe) + $CliArgs
    return (Invoke-NativeCapture -Exe $NodeExe -CmdArgs $nodeArgs)
}

function Get-MediaDurationSec {
    param([string]$FfprobeExe, [string]$MediaPath)
    $res = Invoke-NativeCapture -Exe $FfprobeExe -CmdArgs @("-v", "error", "-show_entries", "format=duration", "-of", "default=nw=1:nk=1", $MediaPath)
    if ($res.ExitCode -ne 0) {
        throw "ffprobe falhou ao ler duracao de '$MediaPath': $($res.Output -join ' ')"
    }
    $raw = ($res.Output -join "`n").Trim()
    return [double]::Parse($raw, $InvCulture)
}

function Get-VideoDimensions {
    param([string]$FfprobeExe, [string]$VideoPath)
    $res = Invoke-NativeCapture -Exe $FfprobeExe -CmdArgs @("-v", "error", "-select_streams", "v:0", "-show_entries", "stream=width,height", "-of", "csv=p=0:s=x", $VideoPath)
    if ($res.ExitCode -ne 0) {
        throw "ffprobe falhou ao ler resolucao de '$VideoPath': $($res.Output -join ' ')"
    }
    $parts = (($res.Output -join "`n").Trim()).Split('x')
    return @{ width = [int]$parts[0]; height = [int]$parts[1] }
}

function Build-EpisodeStoryboard {
    param([string]$EpisodeId)

    if ($EpisodeId -eq "03A") {
        $seg1 = Unescape-Text "M\u00fasica de jogo n\u00e3o pode cortar secamente entre explora\u00e7\u00e3o e combate. Nesta esta\u00e7\u00e3o, combinamos transi\u00e7\u00f5es horizontais na Timeline com uma camada vertical automatizada por CombatIntensity."
        $seg2 = Unescape-Text "Em event:/playground/adaptive_music, a pista principal alterna o loop Exploration, de zero a dezesseis segundos, e Combat, de dezoito a trinta e quatro segundos, suavizados por modula\u00e7\u00e3o AHDSR."
        $seg3 = Unescape-Text "No Logic Deck configuramos cento e vinte BPM, os marcadores Exploration e Combat, os dois Loop Regions infinitos e duas Transition Regions imediatas com crossfades de um v\u00edrgula quatro e um v\u00edrgula seis segundos."
        $seg4 = Unescape-Text "Na pista um est\u00e1 a camada Tension, modulada dinamicamente pela curva global de CombatIntensity, indo de menos vinte e oito a menos dois decib\u00e9is quando a batalha se intensifica."
        $seg5 = Unescape-Text "Agora vamos testar ao vivo. Acionamos Play e abrimos LIVE PARAMS com CombatIntensity em zero, mantendo a m\u00fasica na regi\u00e3o Exploration."
        $seg6 = Unescape-Text "Elevando CombatIntensity para zero ponto oitenta e cinco, o cursor salta suavemente para Combat com crossfade de um v\u00edrgula quatro segundos e entrada da pista Tension."
        $seg7 = Unescape-Text "Reduzindo CombatIntensity de volta para zero, a transi\u00e7\u00e3o reversa retorna para Exploration com crossfade de um v\u00edrgula seis segundos."
        $seg8 = Unescape-Text "No pr\u00f3ximo epis\u00f3dio, vamos integrar esta m\u00fasica adaptativa na Unity usando triggers espaciais da arena de combate."

        return @{
            episodeId = "03A"
            studioProject = "example_project/playground_project.json"
            preSetup = @(
                @{ type = "select"; itemType = "event"; path = "event:/playground/adaptive_music" },
                @{ type = "control"; component_id = "timeline.mode.timeline" },
                @{ type = "set_timeline_zoom"; pixelsPerSecond = 32 },
                @{ type = "ensure_live_params"; open = $false; paramName = "CombatIntensity" },
                @{ type = "clear_highlight" },
                @{ type = "transport"; action = "stop" }
            )
            segments = @(
                @{
                    id = "seg1_intro_timeline"
                    preGapMs = 1200
                    postGapMs = 400
                    text = $seg1
                    actionsAtOffset = @(
                        @{ offsetMs = 300; type = "highlight"; component_id = "timeline_view"; label = "Timeline: Exploration (0-16s), Combat (18-34s) e Tension"; color = "cyan"; dim_opacity = 0.45 }
                    )
                },
                @{
                    id = "seg2_ahdsr_loops"
                    preGapMs = 0
                    postGapMs = 450
                    text = $seg2
                    actionsAtOffset = @()
                },
                @{
                    id = "seg3_logic_deck"
                    preGapMs = 0
                    postGapMs = 450
                    text = $seg3
                    actionsAtOffset = @(
                        @{ offsetMs = -200; type = "control"; component_id = "btn_tab_logic" },
                        @{ offsetMs = -80; type = "highlight"; component_id = "deck_logic"; label = "Logic Deck: 120 BPM, Markers, Loops & Crossfades 1.4s / 1.6s"; color = "amber"; dim_opacity = 0.45 }
                    )
                },
                @{
                    id = "seg4_tension_track"
                    preGapMs = 0
                    postGapMs = 450
                    text = $seg4
                    actionsAtOffset = @(
                        @{ offsetMs = -200; type = "control"; component_id = "timeline.mode.timeline" },
                        @{ offsetMs = -80; type = "highlight"; component_id = "track_lane_1"; label = "Pista Tension: curva global:CombatIntensity (-28 a -2 dB)"; color = "cyan"; dim_opacity = 0.45 }
                    )
                },
                @{
                    id = "seg5_live_exploration"
                    preGapMs = 0
                    postGapMs = 4500
                    text = $seg5
                    actionsAtOffset = @(
                        @{ offsetMs = -260; type = "ensure_live_params"; open = $true; paramName = "CombatIntensity" },
                        @{ offsetMs = -150; type = "set_live_param"; name = "CombatIntensity"; value = 0.0 },
                        @{ offsetMs = -80; type = "highlight"; component_id = "btn_timeline_params_toggle"; label = "LIVE PARAMS: varie CombatIntensity durante Play"; color = "green"; dim_opacity = 0.45 },
                        @{ offsetMs = 0; type = "transport"; action = "play" },
                        @{ offsetMs = 2200; type = "clear_highlight" }
                    )
                },
                @{
                    id = "seg6_live_combat"
                    preGapMs = 0
                    postGapMs = 6500
                    text = $seg6
                    actionsAtOffset = @(
                        @{ offsetMs = 900; type = "set_live_param"; name = "CombatIntensity"; value = 0.85 }
                    )
                },
                @{
                    id = "seg7_live_return_exploration"
                    preGapMs = 0
                    postGapMs = 5000
                    text = $seg7
                    actionsAtOffset = @(
                        @{ offsetMs = 900; type = "set_live_param"; name = "CombatIntensity"; value = 0.0 }
                    )
                },
                @{
                    id = "seg8_outro"
                    preGapMs = 0
                    postGapMs = 1500
                    text = $seg8
                    actionsAtEndOffset = @(
                        @{ offsetMs = 700; type = "transport"; action = "stop" },
                        @{ offsetMs = 820; type = "clear_highlight" }
                    )
                    actionsAtOffset = @()
                }
            )
            zoomPlan = @(
                @{ id = "zoom-tension"; fromSeg = "seg4_tension_track"; startOffsetMs = -150; toSeg = "seg4_tension_track"; endPadMs = 350; target = "track_lane_1"; depth = 1 },
                @{ id = "zoom-combat"; fromSeg = "seg6_live_combat"; startOffsetMs = 500; toSeg = "seg6_live_combat"; endPadMs = 5800; target = "timeline_view"; depth = 1; focusOverride = @{ cx = 0.50; cy = 0.28 } }
            )
        }
    }

    # Construtor generico baseado no manifesto oficial em docs/tutorials/playground/cues/<id>.json
    $cuePath = Join-Path $RepoRoot "docs\tutorials\playground\cues\$EpisodeId.json"
    if (-not (Test-Path -LiteralPath $cuePath)) {
        throw "Manifesto cue nao encontrado para episodio '$EpisodeId': $cuePath"
    }
    $cue = ([System.IO.File]::ReadAllText($cuePath, $Utf8NoBom)) | ConvertFrom-Json
    $studioSteps = @($cue.steps | Where-Object { $_.targetApp -eq "studio" -or $_.targetApp -eq "both" })
    if ($studioSteps.Count -eq 0) {
        throw "Episodio '$EpisodeId' nao possui passos direcionados ao OpenAudio Studio. Use este script para episodios Studio."
    }

    $segments = @()
    foreach ($st in $studioSteps) {
        $acts = @()
        if ($st.studioSelectType -and $st.studioSelectPath) {
            $acts += @{ offsetMs = 0; type = "select"; itemType = $st.studioSelectType; path = $st.studioSelectPath }
        }
        if ($st.studioControlId) {
            $actObj = @{ offsetMs = 60; type = "control"; component_id = $st.studioControlId }
            if ($st.studioControlValue -ne 0) { $actObj.value = $st.studioControlValue }
            $acts += $actObj
        }
        if ($st.studioTransportAction) {
            $acts += @{ offsetMs = 120; type = "transport"; action = $st.studioTransportAction }
        }
        if ($st.studioHighlightId) {
            $acts += @{ offsetMs = 180; type = "highlight"; component_id = $st.studioHighlightId; label = $st.studioHighlightLabel; color = $st.studioHighlightColor; dim_opacity = $st.studioDimOpacity }
        }
        $segments += @{
            id = $st.id
            preGapMs = if ($segments.Count -eq 0) { 1000 } else { 300 }
            postGapMs = 1800
            text = $st.caption
            actionsAtOffset = $acts
        }
    }
    $segments[$segments.Count - 1].actionsAtEndOffset = @(
        @{ offsetMs = 800; type = "transport"; action = "stop" },
        @{ offsetMs = 920; type = "clear_highlight" }
    )
    return @{
        episodeId = $EpisodeId
        studioProject = $cue.studioProject
        preSetup = @(
            @{ type = "select"; itemType = $studioSteps[0].studioSelectType; path = $studioSteps[0].studioSelectPath },
            @{ type = "clear_highlight" },
            @{ type = "transport"; action = "stop" }
        )
        segments = $segments
        zoomPlan = @()
    }
}

function Resolve-NormalizedFocus {
    param(
        [object]$ComponentBounds,
        [object]$RootBounds,
        [object]$CropRegion
    )
    if (-not $ComponentBounds -or -not $RootBounds) {
        return @{ cx = 0.5; cy = 0.5 }
    }
    $rw = [Math]::Max(1.0, [double]$RootBounds.width)
    $rh = [Math]::Max(1.0, [double]$RootBounds.height)
    $compCx = ([double]$ComponentBounds.x + ([double]$ComponentBounds.w / 2.0)) / $rw
    $compCy = ([double]$ComponentBounds.y + ([double]$ComponentBounds.h / 2.0)) / $rh

    if ($CropRegion -and $CropRegion.width -gt 0.01 -and $CropRegion.height -gt 0.01) {
        $compCx = ($compCx - [double]$CropRegion.x) / [double]$CropRegion.width
        $compCy = ($compCy - [double]$CropRegion.y) / [double]$CropRegion.height
    }
    return @{
        cx = [Math]::Round([Math]::Min(0.92, [Math]::Max(0.08, $compCx)), 4)
        cy = [Math]::Round([Math]::Min(0.92, [Math]::Max(0.08, $compCy)), 4)
    }
}

function Wait-FileUnlocked {
    param([string]$FilePath, [int]$MaxAttempts = 15)
    for ($i = 0; $i -lt $MaxAttempts; $i++) {
        if (Test-Path -LiteralPath $FilePath) {
            try {
                $fs = [System.IO.File]::Open($FilePath, 'Open', 'Read', 'ReadWrite')
                $fs.Close()
                return
            } catch {
                Start-Sleep -Milliseconds 200
            }
        } else {
            Start-Sleep -Milliseconds 200
        }
    }
}

Ensure-Win32Interop

$desktopName = [OasRenderWin32]::GetCurrentDesktopName()
if ($CaptureMode -eq "Auto") {
    $effectiveCaptureMode = if ($desktopName -eq "Default") { "OpenScreenLive" } else { "StudioMcp" }
} else {
    $effectiveCaptureMode = $CaptureMode
}

$resolvedOpenscreen = Resolve-ToolPath -Candidate $OpenscreenPath -Fallbacks @(
    (Join-Path $env:LOCALAPPDATA "Programs\Openscreen\Openscreen.exe"),
    "openscreen"
) -FriendlyName "OpenScreen CLI"

$resolvedStudioExe = Resolve-ToolPath -Candidate (Join-Path $RepoRoot $StudioExePath) -Fallbacks @($StudioExePath) -FriendlyName "OpenAudio Studio"
$resolvedFfmpeg = Resolve-ToolPath -Candidate "ffmpeg" -Fallbacks @() -FriendlyName "ffmpeg"
$resolvedFfprobe = Resolve-ToolPath -Candidate "ffprobe" -Fallbacks @() -FriendlyName "ffprobe"
$resolvedNode = Resolve-ToolPath -Candidate "node" -Fallbacks @() -FriendlyName "Node.js"
$resolvedUvx = Resolve-ToolPath -Candidate "uvx" -Fallbacks @((Join-Path $env:USERPROFILE ".local\bin\uvx.exe")) -FriendlyName "uvx (edge-tts)"

$directorScript = Join-Path $ScriptDir "tutorial_openscreen_director.js"
$finalizeScript = Join-Path $ScriptDir "finalize_tutorial_video.ps1"
if (-not (Test-Path -LiteralPath $directorScript)) { throw "Script ausente: $directorScript" }
if (-not (Test-Path -LiteralPath $finalizeScript)) { throw "Script ausente: $finalizeScript" }

$storyboard = Build-EpisodeStoryboard -EpisodeId $Episode
$projectJsonPath = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $storyboard.studioProject))
if (-not (Test-Path -LiteralPath $projectJsonPath)) {
    throw "Projeto Studio nao encontrado: $projectJsonPath"
}

$epOutDir = [System.IO.Path]::GetFullPath((Join-Path (Join-Path $RepoRoot $OutputDir) $Episode))
$workDir = Join-Path $epOutDir "work"
$ttsDir = Join-Path $workDir "tts"
$framesDir = Join-Path $workDir "frames"
$narrationWavPath = Join-Path $epOutDir "${Episode}_narration.wav"
$srtPath = Join-Path $epOutDir "${Episode}.srt"
$openscreenProjPath = Join-Path $epOutDir "${Episode}.openscreen"
$captureMp4Path = Join-Path $epOutDir "${Episode}_capture.mp4"
$finalMp4Path = Join-Path $epOutDir "EP_${Episode}_final.mp4"
$scheduleJsonPath = Join-Path $workDir "${Episode}_schedule.json"
$telemetryJsonPath = Join-Path $workDir "${Episode}_telemetry.json"
$openscreenExportMp4 = Join-Path $workDir "${Episode}_openscreen.mp4"
$mixedMkvPath = Join-Path $workDir "${Episode}_mixed.mkv"

Write-Host "[render_openscreen] Episodio=$Episode | CaptureMode=$effectiveCaptureMode (desktop='$desktopName') | Voice=$Voice"
Write-Host "[render_openscreen] Saida: $finalMp4Path"

if (-not $PSCmdlet.ShouldProcess($finalMp4Path, "Renderizar episodio $Episode via OpenScreen + edge-tts + finalize_tutorial_video")) {
    Write-Host "[render_openscreen] -WhatIf ativo. Plano de execucao:"
    Write-Host "  1. TTS (uvx --from edge-tts edge-tts --voice $Voice) para $($storyboard.segments.Count) segmentos -> $narrationWavPath + $srtPath"
    Write-Host "  2. OpenAudio Studio ($resolvedStudioExe) com projeto $projectJsonPath + $directorScript"
    Write-Host "  3. Gravacao OpenScreen ($resolvedOpenscreen) em modo $effectiveCaptureMode -> $openscreenProjPath"
    Write-Host "  4. Exportacao OpenScreen 60 fps + mixagem sidechain + masterizacao via $finalizeScript -> $finalMp4Path"
    exit 0
}

New-Item -ItemType Directory -Path $epOutDir, $workDir, $ttsDir -Force | Out-Null

# ---------------------------------------------------------------------------
# Etapa 1 — Sintetizar TTS (edge-tts), construir faixa de narracao e SRT
# ---------------------------------------------------------------------------
Write-Host "[render_openscreen] Etapa 1/4: Sintetizando narracao pt-BR ($Voice) e legendas SRT..."
$concatAudEntries = New-Object System.Collections.Generic.List[string]
$srtEntries = New-Object System.Collections.Generic.List[object]
$actionsTimeline = New-Object System.Collections.Generic.List[object]
$segTimings = @{}
$cursorMs = 0
$silCounter = 0

function Add-SilenceSegment {
    param([int]$DurationMs)
    if ($DurationMs -le 0) { return }
    $script:silCounter++
    $silPath = Join-Path $ttsDir ("sil_{0:D2}.wav" -f $script:silCounter)
    $durSecStr = ($DurationMs / 1000.0).ToString("0.000", $InvCulture)
    & $resolvedFfmpeg -hide_banner -loglevel error -y -f lavfi -i "anullsrc=r=48000:cl=stereo" -t $durSecStr -acodec pcm_s16le $silPath
    if ($LASTEXITCODE -ne 0) { throw "Falha ao gerar silencio WAV ($durSecStr s)." }
    $concatAudEntries.Add("file '$($silPath -replace '\\', '/')'")
    $script:cursorMs += $DurationMs
}

for ($i = 0; $i -lt $storyboard.segments.Count; $i++) {
    $seg = $storyboard.segments[$i]
    Add-SilenceSegment -DurationMs ([int]$seg.preGapMs)

    $mp3File = Join-Path $ttsDir ("{0}.mp3" -f $seg.id)
    $wavFile = Join-Path $ttsDir ("{0}.wav" -f $seg.id)
    $txtFile = Join-Path $ttsDir ("{0}.txt" -f $seg.id)
    [System.IO.File]::WriteAllText($txtFile, $seg.text, $Utf8NoBom)

    $ttsRes = Invoke-NativeCapture -Exe $resolvedUvx -CmdArgs @("--from", "edge-tts", "edge-tts", "--voice", $Voice, "--file", $txtFile, "--write-media", $mp3File)
    if ($ttsRes.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $mp3File)) {
        throw "edge-tts falhou no segmento $($seg.id) (exit $($ttsRes.ExitCode)): $($ttsRes.Output -join "`n")"
    }
    & $resolvedFfmpeg -hide_banner -loglevel error -y -i $mp3File -ar 48000 -ac 2 -acodec pcm_s16le $wavFile
    if ($LASTEXITCODE -ne 0) { throw "ffmpeg falhou ao converter '$mp3File' para WAV." }

    $segDurMs = [int][Math]::Round((Get-MediaDurationSec -FfprobeExe $resolvedFfprobe -MediaPath $wavFile) * 1000.0)
    $segStartMs = $cursorMs
    $segEndMs = $segStartMs + $segDurMs

    $segTimings[$seg.id] = @{ startMs = $segStartMs; endMs = $segEndMs; durMs = $segDurMs }
    $concatAudEntries.Add("file '$($wavFile -replace '\\', '/')'")
    $cursorMs = $segEndMs

    foreach ($part in (Split-ToTwoSubtitleParts -Text $seg.text -StartMs $segStartMs -EndMs $segEndMs)) {
        $srtEntries.Add($part)
    }

    foreach ($act in $seg.actionsAtOffset) {
        $obj = @{}
        foreach ($k in $act.Keys) {
            if ($k -ne "offsetMs") { $obj[$k] = $act[$k] }
        }
        $obj["atMs"] = [Math]::Max(0, $segStartMs + [int]$act.offsetMs)
        $actionsTimeline.Add($obj)
    }
    if ($seg.ContainsKey("actionsAtEndOffset") -and $seg.actionsAtEndOffset) {
        foreach ($act in $seg.actionsAtEndOffset) {
            $obj = @{}
            foreach ($k in $act.Keys) {
                if ($k -ne "offsetMs") { $obj[$k] = $act[$k] }
            }
            $obj["atMs"] = [Math]::Max(0, $segEndMs + [int]$act.offsetMs)
            $actionsTimeline.Add($obj)
        }
    }

    Add-SilenceSegment -DurationMs ([int]$seg.postGapMs)
}

$audioConcatFile = Join-Path $ttsDir "concat_audio.txt"
[System.IO.File]::WriteAllText($audioConcatFile, ($concatAudEntries -join "`r`n") + "`r`n", $Utf8NoBom)
& $resolvedFfmpeg -hide_banner -loglevel error -y -f concat -safe 0 -i $audioConcatFile -ar 48000 -ac 2 -acodec pcm_s16le $narrationWavPath
if ($LASTEXITCODE -ne 0) { throw "Falha ao concatenar faixa de narracao." }

$totalNarrationSec = Get-MediaDurationSec -FfprobeExe $resolvedFfprobe -MediaPath $narrationWavPath
$recordDurationSec = [int][Math]::Ceiling($totalNarrationSec) + 2

# Gravar SRT UTF-8 sem BOM
$srtSb = New-Object System.Text.StringBuilder
for ($idx = 0; $idx -lt $srtEntries.Count; $idx++) {
    $entry = $srtEntries[$idx]
    $null = $srtSb.AppendLine([string]($idx + 1))
    $null = $srtSb.AppendLine("$(Format-SrtTime -Ms $entry.startMs) --> $(Format-SrtTime -Ms $entry.endMs)")
    $null = $srtSb.AppendLine($entry.text)
    $null = $srtSb.AppendLine("")
}
[System.IO.File]::WriteAllText($srtPath, $srtSb.ToString(), $Utf8NoBom)

$scheduleObj = @{
    episodeId = $Episode
    preSetup = $storyboard.preSetup
    actions = @($actionsTimeline | Sort-Object { $_.atMs })
}
[System.IO.File]::WriteAllText($scheduleJsonPath, ($scheduleObj | ConvertTo-Json -Depth 8), $Utf8NoBom)

Write-Host ("[render_openscreen] Timeline calculada: narracao={0:0.00}s | gravacao={1}s | legendas={2}" -f $totalNarrationSec, $recordDurationSec, $srtEntries.Count)

# ---------------------------------------------------------------------------
# Etapa 2 — Gravacao ao vivo com OpenAudio Studio + OpenScreen
# ---------------------------------------------------------------------------
Write-Host "[render_openscreen] Etapa 2/4: Abrindo OpenAudio Studio e gravando ($recordDurationSec s)..."

$layoutFile = Join-Path ([System.IO.Path]::GetDirectoryName($resolvedStudioExe)) "oae_studio_layout.json"
$layoutExisted = Test-Path -LiteralPath $layoutFile
$layoutBackup = Join-Path $workDir "oae_studio_layout.backup.json"
if ($layoutExisted) {
    Copy-Item -LiteralPath $layoutFile -Destination $layoutBackup -Force
}

$recDir = Join-Path $env:APPDATA "openscreen\recordings"
$beforeRecs = @()
if (($effectiveCaptureMode -eq "StudioMcp") -and (Test-Path -LiteralPath $recDir)) {
    $beforeRecs = @(Get-ChildItem -LiteralPath $recDir -File | Select-Object -ExpandProperty FullName)
}

$rawRecordProj = if ($effectiveCaptureMode -eq "StudioMcp") {
    Join-Path $workDir "${Episode}_audio_probe.openscreen"
} else {
    $openscreenProjPath
}

$clientRectInfo = $null
$studioProc = $null

try {
    $studioProc = Start-Process -FilePath $resolvedStudioExe -ArgumentList @("`"$projectJsonPath`"") -PassThru
    $hwnd = [IntPtr]::Zero
    for ($i = 0; $i -lt 50; $i++) {
        Start-Sleep -Milliseconds 250
        $hwnd = [OasRenderWin32]::FindMainWindow($studioProc.Id, "OpenAudio Studio")
        if ($hwnd -ne [IntPtr]::Zero) { break }
    }
    if ($hwnd -eq [IntPtr]::Zero) {
        throw "Janela principal 'OpenAudio Studio' nao apareceu dentro do tempo limite."
    }

    $null = [OasRenderWin32]::ShowWindow($hwnd, 3) # SW_MAXIMIZE
    Start-Sleep -Milliseconds 350
    if ($effectiveCaptureMode -eq "OpenScreenLive") {
        $null = [OasRenderWin32]::SetWindowPos($hwnd, (New-Object IntPtr -1), 0, 0, 0, 0, (0x0001 -bor 0x0002 -bor 0x0040))
        Start-Sleep -Milliseconds 250
    }

    $rect = New-Object OasRenderWin32+RECT
    $null = [OasRenderWin32]::GetClientRect($hwnd, [ref]$rect)
    $pt = New-Object OasRenderWin32+POINT
    $pt.X = $rect.Left
    $pt.Y = $rect.Top
    $null = [OasRenderWin32]::ClientToScreen($hwnd, [ref]$pt)
    $clientRectInfo = @{
        left = [int]$pt.X
        top = [int]$pt.Y
        width = [int]($rect.Right - $rect.Left)
        height = [int]($rect.Bottom - $rect.Top)
    }
    Write-Host ("[render_openscreen] Janela Studio clientRect: x={0}, y={1}, w={2}, h={3}" -f $clientRectInfo.left, $clientRectInfo.top, $clientRectInfo.width, $clientRectInfo.height)

    $preRes = Invoke-NativeCapture -Exe $resolvedNode -CmdArgs @($directorScript, "--pre-setup", "--schedule", $scheduleJsonPath, "--wait", "20")
    if ($preRes.ExitCode -ne 0) { throw "Falha no --pre-setup do director MCP: $($preRes.Output -join "`n")" }
    Write-Host "[render_openscreen] Pre-setup MCP OK: $($preRes.Output | Select-Object -Last 1)"

    $dirArgs = @(
        $directorScript,
        "--record-and-direct",
        "--openscreen", $resolvedOpenscreen,
        "--project", $rawRecordProj,
        "--duration", [string]$recordDurationSec,
        "--schedule", $scheduleJsonPath,
        "--telemetry", $telemetryJsonPath,
        "--wait", "20"
    )
    if ($effectiveCaptureMode -eq "StudioMcp") {
        $dirArgs += @("--capture-frames-dir", $framesDir)
    }

    $prevDirEap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $resolvedNode @dirArgs
        $dirExit = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prevDirEap
    }
    if ($dirExit -ne 0) {
        throw "Falha na direcao/gravacao ao vivo (director exit $dirExit)."
    }
} finally {
    if ($studioProc -and -not $studioProc.HasExited) {
        Stop-Process -Id $studioProc.Id -Force -ErrorAction SilentlyContinue
        $studioProc.WaitForExit(5000) | Out-Null
    }
    if (-not $layoutExisted -and (Test-Path -LiteralPath $layoutFile)) {
        Remove-Item -LiteralPath $layoutFile -Force -ErrorAction SilentlyContinue
    } elseif ($layoutExisted -and (Test-Path -LiteralPath $layoutBackup)) {
        Copy-Item -LiteralPath $layoutBackup -Destination $layoutFile -Force
    }
}

$telemetry = ([System.IO.File]::ReadAllText($telemetryJsonPath, $Utf8NoBom)) | ConvertFrom-Json
if ([string]::IsNullOrWhiteSpace($telemetry.rawScreenVideoPath) -or -not (Test-Path -LiteralPath $telemetry.rawScreenVideoPath)) {
    throw "Gravacao bruta do OpenScreen nao encontrada: $($telemetry.rawScreenVideoPath)"
}
Wait-FileUnlocked -FilePath $telemetry.rawScreenVideoPath

# ---------------------------------------------------------------------------
# Etapa 3 — Montar captura (StudioMcp), configurar .openscreen e Exportar
# ---------------------------------------------------------------------------
Write-Host "[render_openscreen] Etapa 3/4: Configurando projeto .openscreen e exportando via OpenScreen compositor..."

$projTemplate = ([System.IO.File]::ReadAllText($rawRecordProj, $Utf8NoBom)) | ConvertFrom-Json
$cropRegion = @{ x = 0.0; y = 0.0; width = 1.0; height = 1.0 }

if ($effectiveCaptureMode -eq "StudioMcp") {
    $frames = @($telemetry.frames)
    if ($frames.Count -lt 2) {
        throw "Numero insuficiente de frames capturados via studio_ui_screenshot: $($frames.Count)"
    }
    $ffconcatSb = New-Object System.Text.StringBuilder
    $null = $ffconcatSb.AppendLine("ffconcat version 1.0")
    $totalMs = ($recordDurationSec + 1) * 1000
    for ($fi = 0; $fi -lt $frames.Count; $fi++) {
        $f = $frames[$fi]
        $fPath = (Join-Path $framesDir $f.file) -replace '\\', '/'
        $nextMs = if ($fi + 1 -lt $frames.Count) { [int]$frames[$fi + 1].tMs } else { [int][Math]::Max($totalMs, ([int]$f.tMs + 200)) }
        $durSec = [Math]::Max(0.017, ($nextMs - [int]$f.tMs) / 1000.0)
        $null = $ffconcatSb.AppendLine("file '$fPath'")
        $null = $ffconcatSb.AppendLine(("duration {0}" -f $durSec.ToString("0.000", $InvCulture)))
    }
    $lastFramePath = (Join-Path $framesDir $frames[$frames.Count - 1].file) -replace '\\', '/'
    $null = $ffconcatSb.AppendLine("file '$lastFramePath'")

    $framesConcatFile = Join-Path $workDir "frames_concat.txt"
    [System.IO.File]::WriteAllText($framesConcatFile, $ffconcatSb.ToString(), $Utf8NoBom)

    # Combinar frames da UI (altura par via crop) + audio WASAPI real do OpenScreen
    & $resolvedFfmpeg -hide_banner -loglevel error -y `
        -f concat -safe 0 -i $framesConcatFile `
        -i $telemetry.rawScreenVideoPath `
        -map "0:v:0" -map "1:a:0" `
        -vf "crop=trunc(iw/2)*2:trunc(ih/2)*2:0:0,fps=60" `
        -c:v libx264 -preset fast -crf 16 -pix_fmt yuv420p `
        -c:a aac -ar 48000 -ac 2 -b:a 192k `
        -shortest $captureMp4Path
    if ($LASTEXITCODE -ne 0) {
        throw "ffmpeg falhou ao montar '$captureMp4Path'."
    }

    # Limpar gravacao bruta temporaria de %APPDATA%\openscreen\recordings\
    if (Test-Path -LiteralPath $recDir) {
        Get-ChildItem -LiteralPath $recDir -File | Where-Object { $beforeRecs -notcontains $_.FullName } | ForEach-Object {
            Remove-Item -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue
        }
    }

    $projTemplate.media.screenVideoPath = $captureMp4Path
} else {
    $rawDims = Get-VideoDimensions -FfprobeExe $resolvedFfprobe -VideoPath $telemetry.rawScreenVideoPath
    $rx = [Math]::Min(0.95, [Math]::Max(0.0, [double]$clientRectInfo.left / [double]$rawDims.width))
    $ry = [Math]::Min(0.95, [Math]::Max(0.0, [double]$clientRectInfo.top / [double]$rawDims.height))
    $rw = [Math]::Min(1.0 - $rx, [Math]::Max(0.05, [double]$clientRectInfo.width / [double]$rawDims.width))
    $rh = [Math]::Min(1.0 - $ry, [Math]::Max(0.05, [double]$clientRectInfo.height / [double]$rawDims.height))
    $cropRegion = @{
        x = [Math]::Round($rx, 4)
        y = [Math]::Round($ry, 4)
        width = [Math]::Round($rw, 4)
        height = [Math]::Round($rh, 4)
    }
}

$zoomRegions = New-Object System.Collections.Generic.List[object]
if ($EnableZoom -and $storyboard.zoomPlan) {
    foreach ($z in $storyboard.zoomPlan) {
        $fromT = $segTimings[$z.fromSeg]
        $toT = $segTimings[$z.toSeg]
        if (-not $fromT -or -not $toT) { continue }
        $zStart = [Math]::Max(0, [int]$fromT.startMs + [int]$z.startOffsetMs)
        $zEnd = [Math]::Max($zStart + 500, [int]$toT.endMs + [int]$z.endPadMs)
        $focus = if ($z.ContainsKey("focusOverride") -and $z.focusOverride) {
            $z.focusOverride
        } else {
            $compProp = $telemetry.componentBounds.PSObject.Properties[$z.target]
            $compBounds = if ($compProp) { $compProp.Value } else { $null }
            Resolve-NormalizedFocus -ComponentBounds $compBounds -RootBounds $telemetry.rootBounds -CropRegion $null
        }
        $zoomRegions.Add(@{
            id = $z.id
            startMs = $zStart
            endMs = $zEnd
            depth = [int]$z.depth
            focus = $focus
            focusMode = "manual"
            source = "manual"
        })
    }
}

$projTemplate.editor.wallpaper = "linear-gradient(135deg, #0b1020 0%, #111827 55%, #0f172a 100%)"
$projTemplate.editor.wallpaperMotion = "none"
$projTemplate.editor.aspectRatio = "16:9"
$projTemplate.editor.padding = $Padding
$projTemplate.editor.borderRadius = $BorderRadius
$projTemplate.editor.shadowIntensity = 0.55
$projTemplate.editor.depthOfField = $false
$projTemplate.editor.autoZoomEnabled = $false
$projTemplate.editor.cropRegion = $cropRegion
$projTemplate.editor.zoomRegions = @($zoomRegions.ToArray())
$projTemplate.editor.exportFormat = "mp4"
$projTemplate.editor.exportQuality = "good"

[System.IO.File]::WriteAllText($openscreenProjPath, ($projTemplate | ConvertTo-Json -Depth 12), $Utf8NoBom)

$infoRes = Invoke-OpenScreenCli -NodeExe $resolvedNode -OpenscreenExe $resolvedOpenscreen -CliArgs @("info", $openscreenProjPath, "--json")
if ($infoRes.ExitCode -ne 0) {
    throw "Openscreen info falhou para '$openscreenProjPath' (exit $($infoRes.ExitCode)): $($infoRes.Output -join "`n")"
}

$exportRes = Invoke-OpenScreenCli -NodeExe $resolvedNode -OpenscreenExe $resolvedOpenscreen -CliArgs @("export", $openscreenProjPath, "-o", $openscreenExportMp4, "--format", "mp4", "--fps", "60", "--quality", "good", "--json")
if ($exportRes.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $openscreenExportMp4)) {
    throw "Openscreen export falhou (exit $($exportRes.ExitCode)): $($exportRes.Output -join "`n")"
}
Write-Host "[render_openscreen] OpenScreen export OK: $openscreenExportMp4"

# ---------------------------------------------------------------------------
# Etapa 4 — Medir audio Studio, aplicar sidechain ducking e finalizar MP4
# ---------------------------------------------------------------------------
Write-Host "[render_openscreen] Etapa 4/4: Mixando narracao + musica real (sidechain) e masterizando..."

$studioRawWav = Join-Path $workDir "${Episode}_studio_raw.wav"
& $resolvedFfmpeg -hide_banner -loglevel error -y -i $openscreenExportMp4 -vn -ar 48000 -ac 2 -acodec pcm_s16le $studioRawWav
if ($LASTEXITCODE -ne 0) { throw "Falha ao extrair audio de '$openscreenExportMp4'." }

$exportDurSec = Get-MediaDurationSec -FfprobeExe $resolvedFfprobe -MediaPath $openscreenExportMp4
$hasPlayWindow = ($null -ne $telemetry.playStartMs)
$playStartSec = if ($hasPlayWindow) { [Math]::Max(0.0, [double]$telemetry.playStartMs / 1000.0) } else { 0.0 }
$playStopSec = if ($null -ne $telemetry.playStopMs) {
    [Math]::Min($exportDurSec, [double]$telemetry.playStopMs / 1000.0)
} else {
    $exportDurSec
}
$probeSpanSec = [Math]::Max(0.5, $playStopSec - $playStartSec)

$prevEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$volOut = & $resolvedFfmpeg -hide_banner -ss ($playStartSec.ToString("0.000", $InvCulture)) -t ($probeSpanSec.ToString("0.000", $InvCulture)) -i $studioRawWav -af volumedetect -f null NUL 2>&1
$ErrorActionPreference = $prevEap

$volText = ($volOut | ForEach-Object { $_.ToString() }) -join "`n"
$maxMatch = [regex]::Match($volText, 'max_volume:\s*([-\d\.inf]+)\s*dB')
$meanMatch = [regex]::Match($volText, 'mean_volume:\s*([-\d\.inf]+)\s*dB')
if (-not $maxMatch.Success -or $maxMatch.Groups[1].Value -eq "-inf") {
    throw "Audio WASAPI do Studio esta completamente silencioso (-inf) no trecho de playback [$playStartSec..$playStopSec s]."
}
$measuredMaxDb = [double]::Parse($maxMatch.Groups[1].Value, $InvCulture)
$measuredMeanDb = if ($meanMatch.Success -and $meanMatch.Groups[1].Value -ne "-inf") { [double]::Parse($meanMatch.Groups[1].Value, $InvCulture) } else { -91.0 }

Write-Host ("[render_openscreen] Audio Studio detectado em [" + $playStartSec.ToString("0.0", $InvCulture) + ".." + $playStopSec.ToString("0.0", $InvCulture) + "s]: mean=" + $measuredMeanDb.ToString("0.1", $InvCulture) + " dB, max=" + $measuredMaxDb.ToString("0.1", $InvCulture) + " dB")
if ($measuredMaxDb -le -85.0) {
    throw "Audio WASAPI do Studio abaixo do limiar minimo (-85 dB): $measuredMaxDb dB."
}

$boostDb = [Math]::Min(42.0, [Math]::Max(0.0, -4.5 - $measuredMaxDb))
$boostDbStr = $boostDb.ToString("0.00", $InvCulture)
$durStr = $exportDurSec.ToString("0.000", $InvCulture)
$fadeInStartStr = $playStartSec.ToString("0.000", $InvCulture)
$fadeOutStartStr = [Math]::Max($playStartSec + 0.1, $playStopSec - 0.25).ToString("0.000", $InvCulture)
$playStopStr = $playStopSec.ToString("0.000", $InvCulture)

$musicGate = if ($hasPlayWindow) {
    "volume='if(between(t,${fadeInStartStr},${playStopStr}),1,0)':eval=frame,afade=t=in:st=${fadeInStartStr}:d=0.15,afade=t=out:st=${fadeOutStartStr}:d=0.20,"
} else {
    ""
}

$filterComplex = "[1:a]aresample=48000,apad=whole_dur=${durStr},asplit=2[narr_mix][narr_sc];[0:a]aresample=48000,${musicGate}volume=${boostDbStr}dB,acompressor=threshold=0.04:ratio=2.2:attack=25:release=300:makeup=2.0,alimiter=limit=0.92[music_pre];[music_pre][narr_sc]sidechaincompress=threshold=0.020:ratio=5:attack=30:release=350:makeup=1[music_ducked];[narr_mix][music_ducked]amix=inputs=2:duration=first:normalize=0[aout]"

& $resolvedFfmpeg -hide_banner -loglevel error -y `
    -i $openscreenExportMp4 `
    -i $narrationWavPath `
    -filter_complex $filterComplex `
    -map "0:v:0" -map "[aout]" `
    -c:v copy -c:a pcm_s16le `
    $mixedMkvPath
if ($LASTEXITCODE -ne 0) {
    throw "Falha na mixagem ffmpeg (boost=${boostDbStr} dB)."
}

$finalizeParams = @{
    InputPath = $mixedMkvPath
    OutputPath = $finalMp4Path
    FfmpegPath = $resolvedFfmpeg
    FfprobePath = $resolvedFfprobe
}
if ($BurnSubtitles) {
    $finalizeParams["Subtitles"] = $srtPath
}
& $finalizeScript @finalizeParams
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $finalMp4Path)) {
    throw "finalize_tutorial_video.ps1 falhou (exit $LASTEXITCODE)."
}

if (-not $KeepWork -and (Test-Path -LiteralPath $workDir)) {
    Remove-Item -LiteralPath $workDir -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "[render_openscreen] Concluido com sucesso!"
Write-Host "  Video final: $finalMp4Path"
Write-Host "  Projeto OpenScreen: $openscreenProjPath"
Write-Host "  Legendas SRT: $srtPath"
Write-Host "  Narracao WAV: $narrationWavPath"
exit 0
