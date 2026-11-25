# Guia Mestre de Produção — Vídeos Tutoriais do Audio Playground (Temporada 1)

Este diretório contém o kit editorial completo e **100% gratuito** para produzir a série oficial de **17 micro-vídeos** (45–90 s cada, Módulos 0 a 6) demonstrando do zero como construir, auditar e integrar o **Audio Playground** no **OpenAudio Studio** (nas visões **Timeline** e **Layers**) e no **Unity 6**.

---

## 1. Por que Captura Externa Gratuita (e não `com.unity.recorder`)

Pacotes internos de gravação da Unity como `com.unity.recorder` **não atendem** a este pipeline por duas razões arquiteturais:

1. **Escopo visual restrito:** o `com.unity.recorder` captura apenas a viewport de renderização (`Game View` ou uma `Camera`), deixando de fora o `Inspector`, a `Hierarchy`, o `Project` e toda a interface desktop do **OpenAudio Studio**.
2. **Roteamento nativo de áudio fora da Unity:** o `OaeAudioManager` inicializa `oae_audio.dll` diretamente no dispositivo de saída WASAPI do sistema operacional (`48000 Hz`, `2` canais), fora do mixer interno / `AudioListener` da Unity. Como resultado, gravadores acoplados apenas ao áudio interno da Unity produzem vídeos sem o som real do motor.

Por isso, o fluxo usa captura gratuita em nível de sistema operacional combinada com destaque cirúrgico nas duas aplicações:

| Ferramenta | Licença | Papel no Pipeline |
| :--- | :--- | :--- |
| **OpenScreen** | MIT (100% gratuito, sem marca d'água) | Captura primária da janela do Studio ou do Unity Editor/PlayMode com cursor suave e auto-zoom nos cliques. |
| **OBS Studio** | GPLv2 | Alternativa/fallback para gravar janela inteira em `1920×1080` 60 fps com **Application Audio Capture (BETA) / WASAPI 48 kHz** isolando `OpenAudio Studio.exe` e `Unity.exe`. |
| **`tutorial_studio_cue.js` + `studio_ui_highlight`** | Repo (MIT) | Direciona seleção de itens, alternância `Timeline`/`Layers`, decks e spotlight (`TutorialHighlightOverlay`) no Studio via TCP `127.0.0.1:7451`. |
| **`PlaygroundTutorialDirectorWindow`** | Repo (MIT) | Diretor nativo no Unity Editor (`OpenAudio/Tutorial Director`) que usa `UnityEditor.Highlighter` no Inspector e aciona cues SmoothStep no `AudioPlaygroundController`. |
| **`finalize_tutorial_video.ps1` (`ffmpeg`)** | LGPL/GPL local | Pós-produção automática em duas passadas (`-14 LUFS`, `-1.0 dBTP`, `48 kHz AAC 192 kbps`), trim e legendas `.srt`. |

---

## 2. Configuração Padronizada de Gravação

- **Resolução e taxa de quadros:** `1920×1080` a `60 fps` (CBR ou CQP `18` em x264 / NVENC).
- **Áudio:** saída do sistema e captura em `48 kHz` estéreo (compatível com `OaeAudioManager` e `StudioEngine`).
- **Escala de interface (legibilidade em mobile):**
  - **Unity 6 Editor:** `Edit -> Preferences -> UI Scaling -> Custom Scaling: 125%`. Mantenha a janela `OpenAudio/Tutorial Director` ancorada em um monitor secundário ou **fora da região capturada**.
  - **OpenAudio Studio:** janela principal maximizada ou `1920×1080`; evite sobrepor terminais sobre a janela durante gravação `--auto` ou `--step`.
- **Projetos de referência:**
  - `example_project/playground_project.json` (Módulos `00A`–`05A`, `05C`, `06A`–`06B`).
  - `example_project/playground_vo_project.json` (Módulo `05B` — `audio_table` + `Programmer Instrument`).
  - Cena Unity: `unity/OpenAudioUnity/Assets/Scenes/AudioPlayground.unity`.

---

## 3. Fluxo de Gravação Passo a Passo

### Passo A — Preparar o OpenAudio Studio (cues automatizados)

1. Inicie o Studio com o servidor MCP habilitado no projeto desejado:
   ```powershell
   & ".\build\Studio_Bundle_Release\OpenAudio Studio.exe" --mcp example_project/playground_project.json
   ```
2. Valide todos os 17 manifestos de cue antes de gravar:
   ```powershell
   node scripts/tutorial_studio_cue.js --validate
   ```
3. Conduza a gravação em modo interativo (`--step`) ou temporizado (`--auto`):
   ```powershell
   # Simular comandos sem abrir o Studio:
   node scripts/tutorial_studio_cue.js --episode 03A --dry-run

   # Avançar manualmente com Enter / repetir (r) / voltar (p) / limpar (c):
   node scripts/tutorial_studio_cue.js --episode 03A --step

   # Executar sequência automática respeitando durationMs:
   node scripts/tutorial_studio_cue.js --episode 03A --auto

   # Remover qualquer destaque ativo:
   node scripts/tutorial_studio_cue.js --clear
   ```

### Passo B — Conduzir o Unity 6 Editor e PlayMode

1. Abra `unity/OpenAudioUnity` no Unity `6000.0.34f1` com a cena `Assets/Scenes/AudioPlayground.unity`.
2. Abra **`OpenAudio -> Tutorial Director`** (`PlaygroundTutorialDirectorWindow`) e selecione o episódio (`00B`, `01C`, `02B`, `03C`, `04B`, `05C`, `06A`, `06B`).
3. Em **EditMode**, clique em **Próximo** / **Repetir Passo** para selecionar o `GameObject` e acionar `UnityEditor.Highlighter.Highlight("Inspector", <label>)` sobre o campo exato.
4. Em **PlayMode**, acione os passos de câmera para executar `BeginCameraMoveCue` (interpolação `SmoothStep` com passos automáticos por superfície e triggers de zona) ou `BeginCameraOrbitCue` (órbita de 360° em torno de `Station2_Emitter3DTotem`). Pressionar `WASD`/Setas ou botão direito cancela o cue imediatamente.

### Passo C — Pós-Produção para YouTube com `ffmpeg`

Normalize loudness para `-14 LUFS` (`48 kHz` AAC 192 kbps) com recorte e queima opcional de legendas:

```powershell
# Simulação segura sem gerar arquivos:
./scripts/finalize_tutorial_video.ps1 -InputPath .\raw\EP_03A.mp4 -OutputPath .\out\EP_03A_final.mp4 -WhatIf

# Execução completa em duas passadas (com trim e legendas opcionais):
./scripts/finalize_tutorial_video.ps1 `
  -InputPath .\raw\EP_03A.mp4 `
  -OutputPath .\out\EP_03A_final.mp4 `
  -Start 00:00:02.500 `
  -End 00:01:18.000 `
  -Subtitles .\subs\EP_03A.srt
```

> **Regra de repositório:** não adicione nem versione arquivos `.mp4`, `.mkv` ou `.mov` no Git.

---

## 4. Estrutura Editorial em 3 Atos (45 s – 90 s)

Cada episódio segue o mesmo arco didático objetivo:

1. **Ato 1 — Objetivo e Conceito (5–10 s):** o problema sonoro real e a técnica que o resolve (sem introduções genéricas longas).
2. **Ato 2 — Configuração Passo a Passo (25–45 s):** abertura dos painéis reais no Studio (`Timeline` e/ou `Layers`) ou campos reais do `Inspector`/C# na Unity, acompanhados por cues `studio_ui_highlight` e `UnityEditor.Highlighter`.
3. **Ato 3 — Prova ao Vivo (15–30 s):** reprodução real ouvindo o áudio do motor nativo, observando medidores K-14, cursores de máscara ou parâmetros 3D reagindo em tempo real.

---

## 5. Grade Completa da Temporada 1 (17 Episódios — Módulos 0 a 6)

| ID | Módulo / Estação | Foco & Visão | Roteiro (3 Atos) | Cues JSON |
| :--- | :--- | :--- | :--- | :--- |
| `00A` | Fundação: Arquitetura do Mixer | Studio (Browser, Mixer, VCAs, F7) | [00A.md](episodes/00A.md) | [00A.json](cues/00A.json) |
| `00B` | Fundação: Setup na Unity | Unity (`OaeAudioManager`, `StudioBankLoader`, `[EventRef]`) | [00B.md](episodes/00B.md) | [00B.json](cues/00B.json) |
| `01A` | Estação 1: Passos Anti-Metralhadora | Studio (**Timeline** + `Multi Instrument` + `Playlist`) | [01A.md](episodes/01A.md) | [01A.json](cues/01A.json) |
| `01B` | Estação 1: Auditoria de Superfícies | Studio (**Layers** + `LIVE PARAMS` + `MaskRangeCanvas`) | [01B.md](episodes/01B.md) | [01B.json](cues/01B.json) |
| `01C` | Estação 1: Pisos Madeira/Concreto/Grama | Unity (`SetGlobalParameter("SurfaceType")` + `PlayOneShot`) | [01C.md](episodes/01C.md) | [01C.json](cues/01C.json) |
| `02A` | Estação 2: Cone 3D e `EventConeAngle` | Studio (`Spatializer3DDeck` + S-curve + alerta `Layers`) | [02A.md](episodes/02A.md) | [02A.json](cues/02A.json) |
| `02B` | Estação 2: Totem 3D e Órbita 360° | Unity (`StudioEventEmitter` + órbita + 5 Built-in 3D Params) | [02B.md](episodes/02B.md) | [02B.json](cues/02B.json) |
| `03A` | Estação 3: Música Adaptativa Horizontal | Studio (**Timeline**: `120 BPM`, Markers, Loops, Transitions, AHDSR) | [03A.md](episodes/03A.md) | [03A.json](cues/03A.json) |
| `03B` | Estação 3: Música Adaptativa Vertical | Studio (**Layers**: `+ Stem Layer`, `Opacity`, `MaskRangeCanvas`, `Fade`) | [03B.md](episodes/03B.md) | [03B.json](cues/03B.json) |
| `03C` | Estação 3: Arena de Combate | Unity (`CombatIntensity` com `seek_speed` + triggers da Arena) | [03C.md](episodes/03C.md) | [03C.json](cues/03C.json) |
| `04A` | Estação 4: Ambiência Estocástica | Studio (`Scatterer Instrument` `async`, polifonia `4`, dispersão `1.5..9 m`) | [04A.md](episodes/04A.md) | [04A.json](cues/04A.json) |
| `04B` | Estação 4: Bosque 3D e Fade-out | Unity (`OaeStopMode.AllowFadeout` no Bosque 3D) | [04B.md](episodes/04B.md) | [04B.json](cues/04B.json) |
| `05A` | Estação 5: Ducking (Sidechain vs Snapshot) | Studio (`Compressor` `sidechain_bus: bus:/vo` vs `vo_ducking` prio `220`) | [05A.md](episodes/05A.md) | [05A.json](cues/05A.json) |
| `05B` | Estação 5: Banco VO e Programmer | Studio (`playground_vo_project.json`, `audio_table`, `DialogueLine`) | [05B.md](episodes/05B.md) | [05B.json](cues/05B.json) |
| `05C` | Estação 5: Túnel da Caverna e Carga Dinâmica | Unity (`LoadBankFile`/`Unload`, `AllowFadeout` sem `Release()` prematuro) | [05C.md](episodes/05C.md) | [05C.json](cues/05C.json) |
| `06A` | Mixagem ao Vivo: Live-Linking | Studio + Unity (bridge TCP `7450`, faders e `Layers` em tempo real) | [06A.md](episodes/06A.md) | [06A.json](cues/06A.json) |
| `06B` | Controles Gerais: VCAs e UI 2D | Unity (`GetVca` na HUD + `ui_click` em `bus:/ui` fora do reverb) | [06B.md](episodes/06B.md) | [06B.json](cues/06B.json) |

---

## 6. Checklist de Publicação (YouTube)

- [ ] Vídeo em `1920×1080` `60 fps` gerado via OpenScreen (MIT) ou OBS Studio sem marca d'água.
- [ ] Áudio normalizado por `scripts/finalize_tutorial_video.ps1` confirmado via `ffprobe` em `48 kHz` AAC, `-14 LUFS` (`TP <= -1.0 dBTP`).
- [ ] Nenhum destaque residual (`studio_ui_highlight { clear: true }` e `Highlighter.Stop()`) nem janela do `Tutorial Director` visível no quadro final.
- [ ] Descrição do vídeo inclui capítulos (`00:00 Ato 1 — Objetivo`, `00:10 Ato 2 — Configuração`, `... Ato 3 — Prova ao Vivo`), referência ao repositório `OpenAudioStudio` e número exato do episódio (`EP 00A`..`06B`).
- [ ] Legendas `.srt` revisadas para termos técnicos (`EventConeAngle`, `Multi Instrument`, `MaskRangeCanvas`, `AllowFadeout`, `Live-Linking`).

---

## 7. Escopo Futuro — Módulo 7 / Temporada 2 ("Receitas de Efeitos Sonoros")

Os tópicos avançados abaixo estão explicitamente reservados para a **Temporada 2 (Módulo 7)**, fora do escopo da Temporada 1:

- Oclusão e filtragem submersa controlada por parâmetro contínuo.
- Cadeia de efeito rádio / *walkie-talkie* (bandpass + distorção/compressão).
- Delay rítmico e reverberação dedicados via *Send/Return*.
- Carga de arma/habilidade com `Sustain Point` + disparo de `Key Off`.
- `Command Instrument` para eventos compostos e efeitos de *slow-motion*.
- Eventos aninhados compostos e integração `OaeEventTrack` / `OaeEventClip` na Unity Timeline.
