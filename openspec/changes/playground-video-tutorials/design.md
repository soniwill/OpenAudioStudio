# Design Técnico: Série Completa de Vídeos Tutoriais do Audio Playground (`playground-video-tutorials`)

## Context

Consulte [proposal.md](proposal.md) para motivação e escopo. Do ponto de vista técnico, existe uma assimetria clara entre os dois aplicativos envolvidos nos tutoriais:

- **OpenAudio Studio:** janela desktop JUCE que suporta inicialização simultânea da interface real, dispositivo de áudio (`engine_.init_device()`), Live-Linking TCP (`127.0.0.1:7450`) e servidor MCP (`127.0.0.1:7451`) via `"OpenAudio Studio.exe" --mcp <projeto.json>`. Os componentes relevantes já possuem `component_id` semânticos na `TimelinePanel`, na `LayerStackView`, no `AuthoringTreeBrowser`, no `InspectorPanel` e no `ContextualBottomDeck`, e `studio_ui_inspect` devolve `bounds: { x, y, w, h }` em coordenadas locais do `MainComponent` (`root_component`). Contudo, não há uma camada visual nativa que aplique efeito *spotlight* (escurecimento periférico + contorno + badge) durante gravações ao vivo.
- **Unity Editor (`6000.0.34f1`, Built-in RP):** o `Packages/manifest.json` não possui `com.unity.recorder`, `Cinemachine` nem `com.unity.learn.iet-framework`. Além de gravadores internos capturarem apenas a `Game View` (sem `Inspector` nem `Hierarchy`), o `OaeAudioManager` inicializa o motor nativo `oae_audio.dll` diretamente em dispositivo de saída do sistema fora do `AudioListener` nativo da Unity. Por isso, a captura de vídeo e áudio precisa ocorrer no nível de janela do sistema operacional (via **OpenScreen** MIT ou **OBS Studio** com *Application Audio Capture* / WASAPI em 48 kHz), combinada com destaque nativo de campos via `UnityEditor.Highlighter` e movimentação programática suave no `AudioPlaygroundController` (movido inteiramente por `Transform` em `m_PlayerTransform`, sem física/colisores).

## Goals / Non-Goals

**Goals:**
- Viabilizar gravação em tempo real, 100% gratuita e sem marca d'água, com destaque preciso de controles no Studio (Timeline e Layers) e no Unity 6 Editor/PlayMode, mantendo o áudio nativo da engine sincronizado.
- Compartilhar um esquema JSON único e plano entre `scripts/tutorial_studio_cue.js` (Node.js) e `PlaygroundTutorialDirectorWindow.cs` (`UnityEngine.JsonUtility`).
- Cobrir integralmente todos os **17 micro-vídeos da Temporada 1 (Módulos 0 a 6)** com roteiros em 3 atos e técnicas explícitas de design de áudio (Multi-Instrumento anti-metralhadora, Ducking por Sidechain vs. Snapshot, Espacialização 3D com cone, Música Adaptativa em Timeline vs. Layers, Scatterer, Banco sob demanda + Programmer Instrument e Live-Linking).

**Non-Goals:**
- Produzir ou versionar arquivos `.mp4` ou mídias binárias de vídeo no Git.
- Implementar os roteiros do **Módulo 7 / Temporada 2** ("Receitas de Efeitos Sonoros": Oclusão, Rádio/Walkie-Talkie, Delay em Send/Return, Carga + `Sustain`/`Key Off`, `Command Instrument` slow-mo, Eventos Aninhados e Unity Timeline).
- Adicionar pacotes externos ao `unity/OpenAudioUnity/Packages/manifest.json`.
- Alterar a ABI C, formato `.bank`, ou os JSONs dos projetos do Playground (`playground_project.json` e `playground_vo_project.json`).

## Arquitetura do Pipeline de Produção

```text
+-----------------------------------------------------------------------------+
|            PIPELINE 100% GRATUITO DE PRODUCAO (TEMPORADA 1 - 17 EP)         |
+-----------------------------------------------------------------------------+

  [Roteiros 00A..06B (3 Atos)] + [Cues JSON planos (docs/tutorials/playground/cues/)]
        |                                                       |
        v                                                       v
  +-------------------------------+       +-----------------------------------+
  | STUDIO DIRECTOR (Node.js)     |       | UNITY DIRECTOR (C# Editor-Only)   |
  | scripts/tutorial_studio_cue.js|       | PlaygroundTutorialDirectorWindow  |
  |  - TCP MCP 127.0.0.1:7451     |       |  - Selection.activeGameObject     |
  |  - studio_select_item /       |       |  - UnityEditor.Highlighter        |
  |    studio_transport /         |       |    .Highlight("Inspector", label) |
  |    studio_ui_highlight        |       |  - PlayMode: AudioPlaygroundCtrl  |
  |  - TutorialHighlightOverlay   |       |    BeginCameraMoveCue / OrbitCue  |
  |    (z-top em MainComponent)   |       |    (Transform SmoothStep + Steps) |
  +---------------+---------------+       +-----------------+-----------------+
                  |                                         |
                  +--------------------+--------------------+
                                       v
           +-------------------------------------------------------+
           | CAPTURA EXTERNA GRATUITA (1080p 60fps + 48 kHz WASAPI)|
           |  - OpenScreen (MIT): auto-zoom cliques + cursor suave |
           |  - OBS Studio (GPL): fallback Application Audio Cap.  |
           +---------------------------+---------------------------+
                                       v
           +-------------------------------------------------------+
           | POS-PROCESSAMENTO (scripts/finalize_tutorial_video.ps1)|
           |  - ffmpeg 2 passadas loudnorm -14 LUFS / TP -1.0      |
           |  - trim opcional (-Start / -End) e legendas (-Subtitles)|
           +---------------------------+---------------------------+
                                       v
                     [EP_00A .. EP_06B prontos para YouTube]
```

## Decisions

### 1. `TutorialHighlightOverlay` em JUCE (`studio/Source/UI/TutorialHighlightOverlay.h/.cpp`)
- **Estrutura e Posicionamento:** componente filho direto de `MainComponent` cobrindo integralmente `MainComponent::getLocalBounds()`. Como `MainComponent::layout_workspace_components()` (`MainComponent.cpp:898-934`) cria e insere dinamicamente `slotHeaders_` e `workspaceSplitters_` via `addAndMakeVisible(...)`, o overlay é reajustado e trazido ao topo com `toFront(false)` ao final de `layout_workspace_components()` e `resized()`.
- **Transparência Total de Input:** sobrescreve `bool hitTest(int, int) override { return false; }` e configura `setInterceptsMouseClicks(false, false)` + `setWantsKeyboardFocus(false)`. Assim, chamadas `ctx_.root_component->getComponentAt(...)` em `McpServer::tool_ui_mouse` (`McpServer.cpp:905`) e `McpServer::tool_ui_set_control` (`McpServer.cpp:1001`) atravessam o overlay e atingem os controles reais abaixo dele.
- **Renderização do Spotlight:**
  - Calcula em `paint()` (ou ao atualizar alvo) o retângulo local `targetRect` do componente alvo relativo ao `MainComponent`, com padding de `4 px`. Caso o componente tenha sido destruído, ocultado (`!isShowing()`) ou ficado com `width <= 0 || height <= 0`, o overlay ignora o desenho (ou se oculta com segurança).
  - Desenha o escurecimento periférico (`bgDark` com `dim_opacity`) como **4 retângulos não sobrepostos** ao redor de `targetRect` (topo, base, esquerda e direita), evitando criação de máscaras alocadas em tempo real.
  - Desenha o contorno brilhante arredondado usando tokens de `Theme.h` mapeados pelo parâmetro `color`: `cyan` -> `OaeTheme::Colors::accentCyan`, `amber` -> `OaeTheme::Colors::accentAmber`, `green` -> `OaeTheme::Colors::accentGreen`.
  - Quando `label` não está vazio, posiciona uma pílula compacta (`bgSurfaceRaised` + borda na cor ativa + `textPrimary`) preferencialmente acima de `targetRect`, caindo para abaixo, direita ou dentro da área útil de `getLocalBounds()` para nunca ser cortada nas bordas da janela.
- **Estado Inativo e `studio_ui_inspect`:** quando `clear()` é chamado (ou no estado inicial), executa `setVisible(false)` e `repaint()`, não aparecendo em `studio_ui_inspect` com `visible_only: true` nem interferindo no uso cotidiano do Studio.
- **Alternativa descartada:** desenhar somente dentro de `MainComponent::paintOverChildren()` sem componente dedicado tornaria mais frágil o isolamento de estado, geometria da pílula e testes diretos de componente.

### 2. Ferramenta MCP `studio_ui_highlight` (`McpProtocol.h` e `McpServer.h/.cpp`)
- Torna-se a **13ª ferramenta** registrada em `oae::studio::mcp::get_tools_catalog()` e despachada em `McpServer::handle_tools_call` (`McpServer.cpp:383-408`) via `run_on_message_thread(...)`.
- **Contrato de Entrada:**
  - `component_id` (`string`): obrigatório quando `clear` é falso; identifica o componente alvo na hierarquia do `root_component`.
  - `label` (`string`, opcional): legenda curta exibida junto ao contorno em destaque.
  - `dim_opacity` (`number`, opcional, padrão `0.45`, clampado em `[0.0, 0.85]`): opacidade do escurecimento ao redor do alvo.
  - `color` (`string`, opcional, enum `["cyan", "amber", "green"]`, padrão `"cyan"`).
  - `clear` (`boolean`, opcional, padrão `false`): quando `true`, desativa o overlay e ignora `component_id`.
- **Respostas e Captura:** retorna JSON `{ active, component_id, bounds: { x, y, w, h }, label, dim_opacity, color }` em ativação, `{ active: false }` em limpeza, e `isError: true` se `clear` for falso e `component_id` estiver ausente, não existir ou possuir `width <= 0 || height <= 0`. Por ser filho visível de `MainComponent`, `studio_ui_screenshot` sem `component_id` (`McpServer.cpp:1118`) captura automaticamente o destaque ativo.
- **Armadilha mapeada:** verificar `run_smoke_mcp()` em `studio/Source/Main.cpp` (`tools.size() >= 6` e asserções de catálogo) e adicionar teste unitário para `studio_ui_highlight`, além de conferir se `docs/specs/mcp-server-guide.md` ou `docs/specs/mcp-protocol.md` precisam registrar a nova ferramenta na etapa de implementação.

### 3. Esquema Único e Plano de Cue JSON (`UnityEngine.JsonUtility` + Node.js)
Para evitar dois formatos divergentes de roteiro automatizado entre `scripts/tutorial_studio_cue.js` e `PlaygroundTutorialDirectorWindow.cs`, todos os arquivos em `docs/tutorials/playground/cues/<ep>.json` adotam estritamente **classes/objetos planos sem dicionários nem arrays polimórficos**, compatíveis com `UnityEngine.JsonUtility.FromJson<EpisodeCueManifest>(json)`:

```json
{
  "episodeId": "03A",
  "title": "Estacao 3: Musica Adaptativa na Timeline",
  "studioProject": "example_project/playground_project.json",
  "unityScene": "Assets/Scenes/AudioPlayground.unity",
  "steps": [
    {
      "id": "03A-1",
      "targetApp": "studio",
      "durationMs": 4500,
      "caption": "Selecione event:/playground/adaptive_music na Timeline",
      "studioSelectType": "event",
      "studioSelectPath": "event:/playground/adaptive_music",
      "studioHighlightId": "timeline_view",
      "studioHighlightLabel": "Timeline: Exploration (0-16s) e Combat (18-34s)",
      "studioHighlightColor": "cyan",
      "studioDimOpacity": 0.45,
      "studioTransportAction": "play",
      "studioControlId": "",
      "studioControlValue": 0.0,
      "unityMode": "",
      "unityGameObject": "",
      "unityInspectorLabel": "",
      "unityCameraCue": "",
      "unityDurationSec": 0.0
    }
  ]
}
```

- Campos de string vazios (`""`) ou numéricos `0.0` indicam no-op na respectiva etapa; campos `studio*` são executados por `scripts/tutorial_studio_cue.js` e campos `unity*` pelo `PlaygroundTutorialDirectorWindow.cs`.

### 4. `PlaygroundTutorialDirectorWindow` no Unity Editor (`unity/com.openaudio.core/Editor/`)
- **Menu:** `[MenuItem("OpenAudio/Tutorial Director")]` em `OpenAudio.Editor.asmdef` (nenhuma referência vaza para `OpenAudio.Runtime` ou compilação `.NET 8`).
- **Controles da Janela (deve permanecer fora da janela gravada ou acoplada fora da região de captura):**
  - Seletor de episódio (`00A`..`06B` carregados de `docs/tutorials/playground/cues/`).
  - Lista clicável de passos + botões `Anterior`, `Próximo`, `Repetir Passo` e `Parar Destaque`, exibindo a legenda (`caption`) da etapa atual para orientação do apresentador durante a gravação.
- **Execução EditMode:** resolve o `GameObject` na cena (`AudioPlayground_Controller`, `OpenAudio_AudioManager`, `Main Camera`, `Station2_Emitter3DTotem`), atribui `Selection.activeGameObject`, garante foco da janela `Inspector` (`EditorApplication.ExecuteMenuItem("Window/General/Inspector")` ou tipo `UnityEditor.InspectorWindow`) e chama `Highlighter.Highlight("Inspector", step.unityInspectorLabel)`.
- **Mapeamento verificado dos rótulos reais do Inspector (sem achismos):**
  - `StudioBankLoader.cs`: `"Bank Files"`, `"Load Mode"`, `"Load Trigger"`, `"Unload Trigger"`, `"Preload Sample Data"`.
  - `OaeAudioManager.cs`: `"Sample Rate"`, `"Max Physical Voices"`, `"Enable Live Linking"`, `"Live Host"`, `"Live Port"`.
  - `StudioEventEmitter.cs`: `"Event Path"`, `"Play Trigger"`, `"Stop Trigger"`, `"Stop Mode"`, `"Spatial Mode"`, `"Min Distance"`, `"Max Distance"`, `"Rolloff"`, `"Sound Size"`, `"Min Extent"`, `"Cone Inner Angle"`, `"Cone Outer Angle"`, `"Cone Outer Gain"`.
  - `OaeDiagnosticsOverlay.cs`: `"Toggle Key"`, `"Watched Buses"`.
  - `AudioPlaygroundController.cs`: `"Footstep Event Path"`, `"Emitter 3D Event Path"`, `"Adaptive Music Event Path"`, `"Forest Scatterer Event Path"`, `"Dialogue Vo Event Path"`, `"Ui Click Event Path"`, `"Cave Snapshot Path"`, `"Vo Ducking Snapshot Path"`, `"Master Vca Path"`, `"Music Vca Path"`, `"Sfx Vca Path"`, `"Vo Bank File Name"`.
- **Alternativa descartada:** instalar `com.unity.learn.iet-framework` — rejeitado pelo usuário para manter `Packages/manifest.json` intacto e zero dependências extras.

### 5. Movimentação e Câmera Suaves no `AudioPlaygroundController.cs`
- Como `AudioPlaygroundController.cs` já movimenta o jogador diretamente via `m_PlayerTransform` (`AudioPlaygroundController.cs:841-914`), sem `CharacterController` nem `Rigidbody`, adicionamos um estado interno leve de cue de câmera (compilado tanto em `#if UNITY_5_3_OR_NEWER` quanto em `.NET 8` usando estado determinístico testável):
  - `BeginCameraMoveCue(float targetX, float targetY, float targetZ, float targetYawDeg, float targetPitchDeg, float durationSeconds)`: interpola posição, yaw e pitch usando curva `SmoothStep(t)` (`t * t * (3 - 2 * t)`), atualiza `StudioListener` (via `m_PlayerTransform`) e mantém ativos `DetectSurfaceAtPosition` + disparo rítmico de passos e `UpdateZoneTriggersFromPosition`.
  - `BeginCameraOrbitCue(float centerX, float centerY, float centerZ, float radius, float height, float durationSeconds, float startAngleDeg = 180f, float sweepDeg = 360f)`: executa órbita circular apontando constantemente para o centro informado — ideal para rodear `Station2_Emitter3DTotem` em `(0.0f, 1.2f, 8.0f)` exibindo `EventConeAngle` variar de `0°` (frente) para `180°` (costas) e voltar a `0°`.
  - `CancelCameraCue()` / `IsCameraCueActive`: cancelamento explícito ou automático assim que o usuário pressionar `WASD`/Setas ou botão direito do mouse durante a gravação.
- **Coordenadas exatas de referência do Playground (para os cues e presets do Director):**
  - Spawn: `(0.0, 1.6, -4.0)`, `yaw=0`, `pitch=5`
  - Estação 1 (Wood / Concrete / Grass): `(-4.0, 1.6, 0.0)`, `(0.0, 1.6, 0.0)`, `(4.0, 1.6, 0.0)`
  - Estação 2 (Emissor 3D Totem em `(0.0, 1.2, 8.0)`): posição frontal `(0.0, 1.6, 4.5)` e órbita raio `3.6 m`
  - Estação 3 (Arena de Combate): centro `(-7.5, 1.6, 6.5)`
  - Estação 4 (Bosque Scatterer): centro `(7.5, 1.6, 6.5)`
  - Estação 5 (Caverna + Kiosk VO): entrada/centro `(0.0, 1.6, 14.2)` olhando para o kiosk `(1.9, 1.2, 13.6)`

### 6. Grade Curricular Completa dos 17 Episódios (Temporada 1: Módulos 0 a 6)

Todos os roteiros em `docs/tutorials/playground/` seguem a estrutura de **3 atos** em 45 s–90 s: **Ato 1 — Objetivo/Conceito (5–10 s)** -> **Ato 2 — Configuração passo a passo com destaques (25–45 s)** -> **Ato 3 — Prova ao vivo ouvindo o áudio real (15–30 s)**.

| ID | Módulo / Estação | Ambiente & Visão | Técnicas, Controles em Foco e Resultados Auditivos |
| :--- | :--- | :--- | :--- |
| `00A` | Fundação: Arquitetura do Mixer | Studio (Browser, Mixer Deck, Project Toolbar) | Estrutura `bus:/` (Master Limiter), `sfx`, `music`, `ambience`, `vo`, `ui`, 3 VCAs (`playground_master`/`music`/`sfx`) e `btn_build_bank` (`F7`). |
| `00B` | Fundação: Setup na Unity | Unity (EditMode Inspector) | `OaeAudioManager`, `StudioBankLoader` (`playground.bank`), `StudioListener`, `OaeDiagnosticsOverlay` (`F9`) e `[EventRef]` -> `Browse...` (`CatalogCache`). |
| `01A` | Estação 1: Passos Anti-Metralhadora | Studio (**Timeline**) | Parâmetro global `SurfaceType` (`Wood/Concrete/Grass`), 3 `Multi Instrument` (`random_no_repeat`, pesos, randomização `±1.8 st` / `±2 dB`), `cooldown_ms: 40`, `steal_mode: oldest`. |
| `01B` | Estação 1: Auditoria de Superfícies | Studio (**Layers**) | Alternar para `timeline.mode.layers`, inspecionar stems por faixa de `SurfaceType` na `MaskRangeCanvas`, usar `TimelineParameterStrip` (`LIVE PARAMS`) e `status` ao vivo. |
| `01C` | Estação 1: Pisos Madeira/Concreto/Grama | Unity (EditMode + PlayMode) | Campos `Footstep Event Path` / `Step Interval Seconds`, código `SetGlobalParameter("SurfaceType", ...)` + `PlayOneShot` e glide suave nos 3 pisos da Estação 1. |
| `02A` | Estação 2: Cone 3D e Built-in Params | Studio (Deck 3D + Inspector + Layers) | `Spatializer3DDeck` (`deck_spatializer`), parâmetro local `EventConeAngle` + curva S-curve (`0°/45°/115°/180°`) e aviso `"Curva Personalizada"` -> `"Editar na Timeline"` no `Layers`. |
| `02B` | Estação 2: Totem 3D e Órbita 360° | Unity (Inspector + PlayMode) | `StudioEventEmitter` no `Station2_Emitter3DTotem` (`Spatial3D`, `Inverse`, `1.5..22 m`, `SoundSize 1.2`, cone `45°/115°`, `ConeOuterGain 0.04`) + órbita 360° lendo os 5 Built-in 3D Params. |
| `03A` | Estação 3: Música Adaptativa Horizontal | Studio (**Timeline**) | `Tempo 120 BPM`, Markers `Exploration`/`Combat`, `Loop Region`, `Transition Region` (`1,4 s`/`1,6 s`), modulador `AHDSR` e pista `Tension` automatizada por `CombatIntensity`. |
| `03B` | Estação 3: Música Adaptativa Vertical | Studio (**Layers**) | `+ Stem Layer`, `Opacity`, `MaskRangeCanvas` (`[min..max]` + `Fade`), `MaskTransitionCard` no Inspector e cursor vertical cruzando `AGUARDANDO -> TOCANDO`. |
| `03C` | Estação 3: Arena de Combate | Unity (EditMode + PlayMode) | `Adaptive Music Event Path`, `CreateInstance -> Start` + `SetGlobalParameter("CombatIntensity", ...)` com `seek_speed`, entrada/saída suave na Arena. |
| `04A` | Estação 4: Ambiência Estocástica | Studio (Timeline + Playlist + Inspector) | `Scatterer Instrument` (`async`, `length 6000 ms`, `spawn 220..620 ms`, `polyphony 4`, dispersão `1.5..9.0 m`, 4 grãos ponderados + `Loop Region`). |
| `04B` | Estação 4: Bosque 3D e Fade-out | Unity (EditMode + PlayMode) | `Forest Scatterer Event Path`, posicionamento 3D no bosque (`(7.0, 1.5, 6.0)`), parada suave via `Stop(OaeStopMode.AllowFadeout)`. |
| `05A` | Estação 5: Ducking (Sidechain vs Snapshot) | Studio (Mixer, Effects, Layers) | Comparativo prático de **Ducking**: (1) dinâmico por `Compressor` com `sidechain_bus: bus:/vo` em `bus:/music`, (2) determinístico por `snapshot:/playground/vo_ducking` (prioridade `220`) sobre `cave_reverb` (`180`), `Reverb`/`Lowpass` e `AdjustmentLayerCard` em `Layers`. |
| `05B` | Estação 5: Banco VO e Programmer | Studio (Project VO + Inspector) | Projeto `playground_vo_project.json`, edição da `audio_table` (`vo_welcome`, `vo_spatial_test`, `vo_ducking_active`), parâmetro `DialogueLine` e `Programmer Instrument`. |
| `05C` | Estação 5: Túnel da Caverna e Carga Dinâmica | Unity (Inspector + PlayMode) | `Cave Snapshot Path`, `Vo Ducking Snapshot Path`, `Vo Bank File Name`, `LoadBankFile`/`Unload`, `SetParameterWithLabel` e por que NÃO chamar `Release()` prematuro antes do fade-out. |
| `06A` | Mixagem ao Vivo: Live-Linking | Studio + Unity (lado a lado / alternado) | Ponte TCP `7450` conectada (`Live: Unity Connected`), ajustar faders no `MixerDeckPanel` e máscaras no `Layers` ouvindo o Playground reagir instantaneamente. |
| `06B` | Controles Gerais: VCAs e UI 2D | Unity (Inspector + HUD PlayMode) | `Master/Music/Sfx Vca Path`, `GetVca(path)` na HUD, e `event:/playground/ui_click` roteado em `bus:/ui` fora do reverb de `bus:/sfx`. |

### 7. Pós-Processamento Gratuito com `ffmpeg` (`scripts/finalize_tutorial_video.ps1`)
- Usa o `ffmpeg` já instalado para garantir padrão consistente de volume e qualidade entre episódios gravados no OpenScreen ou OBS:
  - Duas passadas de `loudnorm=I=-14:TP=-1.0:LRA=11` (padrão recomendado para YouTube, preservando transientes sem clipar em `-1 dBTP`), áudio em 48 kHz AAC 192 kbps.
  - Suporte a corte preciso (`-Start`, `-End`), burn-in opcional de legendas `.srt` (`-Subtitles`) e simulação segura sem escrita (`-WhatIf` / impressão da linha de comando planejada).

## Risks / Trade-offs

- **[Risco] Variação de rótulos internos do `UnityEditor.Highlighter` entre subversões do Unity 6** -> **Mitigação:** validar cada string `unityInspectorLabel` diretamente no código fonte dos componentes OpenAudio (`StudioBankLoader.cs`, `StudioEventEmitter.cs`, `OaeAudioManager.cs`, `OaeDiagnosticsOverlay.cs`, `AudioPlaygroundController.cs`) no Unity `6000.0.34f1` e manter a execução do `Highlighter` defensiva (logando aviso amigável se `Highlighter.Highlight(...)` retornar `false` sem lançar exceção).
- **[Risco] Limitações do `UnityEngine.JsonUtility` em arrays polimórficos** -> **Mitigação:** impor esquema JSON estritamente plano com tipos primitivos e validar os 17 arquivos de cue em CI via `node scripts/tutorial_studio_cue.js --validate`.
- **[Risco] Captura em escala de DPI diferente de 100% no Windows distorcendo legibilidade em telas pequenas** -> **Mitigação:** padronizar no guia `docs/tutorials/playground/README.md` resolução de gravação `1920×1080` a `60 fps`, escala de fonte do Unity Editor em `125%`, e uso do zoom automático do OpenScreen apenas nos momentos de foco.
- **[Risco] Quebra de verificação em `--smoke-mcp` ou divergência na contagem de ferramentas MCP** -> **Mitigação:** atualizar as asserções de `run_smoke_mcp()` em `studio/Source/Main.cpp` para validar a presença de `studio_ui_highlight` e seu ciclo ativar/screenshot/limpar/erro.
