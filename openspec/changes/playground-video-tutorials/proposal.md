# Proposal: Série Completa de Vídeos Tutoriais do Audio Playground (`playground-video-tutorials`)

## Motivação

A cena `AudioPlayground.unity` (`add-unity-audio-playground`) reúne as principais capacidades de áudio interativo do OpenAudio Studio e da ponte Unity em cinco estações 3D, porém ainda não conta com material audiovisual passo a passo que ensine sound designers e desenvolvedores Unity (incluindo um canal oficial no YouTube) a construir cada etapa do zero — tanto na visão clássica de **Timeline** quanto na visão **Layers** (`LayerStackView`) e na integração no **Unity Editor**.

Adicionalmente, pacotes internos de captura como `com.unity.recorder` não atendem a esse caso de uso por duas razões técnicas: (1) capturam apenas a viewport de renderização (`Game View` / câmeras), sem gravar o `Inspector`, a `Hierarchy` nem a interface do OpenAudio Studio; e (2) o `OaeAudioManager` inicializa `oae_audio.dll` em dispositivo de saída próprio fora do `AudioListener` interno da Unity, fazendo com que gravadores ligados apenas ao mixer interno da Unity produzam vídeos sem áudio do motor. É necessário, portanto, um kit totalmente gratuito baseado em captura externa de janela/áudio do sistema (OpenScreen MIT / OBS Studio) combinado com destaque cirúrgico de controles em ambas as aplicações, movimentação suave de câmera na Unity e roteiros curtos por episódio.

## Escopo (Temporada 1 — 17 Micro-Vídeos, Módulos 0 a 6)

Esta change constrói exclusivamente sobre `add-unity-audio-playground` (sem conflitar com suas entregas) e entrega quatro pilares integrados:

1. **Kit Editorial, Roteiros e Cues da Temporada 1 (`docs/tutorials/playground/`)**:
   - Guia mestre `docs/tutorials/playground/README.md` documentando o pipeline 100% gratuito (OpenScreen sob licença MIT e OBS Studio via *Application Audio Capture* / WASAPI em 48 kHz, 1080p 60 fps, escala de UI e checklist de publicação para YouTube), explicando por que `com.unity.recorder` não captura a UI do Editor nem a saída nativa de `oae_audio.dll`.
   - **17 roteiros curtos (45 s–90 s cada)** cobrindo os **Módulos 0 a 6** no formato em 3 atos (*Objetivo -> Configuração -> Resultado ao vivo*), incluindo ensino explícito das técnicas de áudio subjacentes:
     - **Módulo 0 — Fundação (`00A` Studio, `00B` Unity):** topologia de buses, Master Limiter, VCAs, `Build Bank (F7)`, `OaeAudioManager`, `StudioBankLoader`, `StudioListener`, `OaeDiagnosticsOverlay` e `[EventRef]` / `Browse...`.
     - **Módulo 1 — Estação 1 (`01A` Timeline, `01B` Layers, `01C` Unity):** passos por superfície com `SurfaceType`, prevenção de efeito metralhadora via `Multi Instrument` (`random_no_repeat`, randomização de pitch/volume, `cooldown_ms`, `steal_mode: oldest`), auditoria em `Layers` + `TimelineParameterStrip` e `SetGlobalParameter` + `PlayOneShot`.
     - **Módulo 2 — Estação 2 (`02A` Studio, `02B` Unity):** espacialização 3D no `Spatializer3DDeck`, curva S-curve no parâmetro built-in `EventConeAngle` (e alerta `"Curva Personalizada"` no `Layers`), `StudioEventEmitter` (cone `45°/115°`, `ConeOuterGain 0.04`, `Rolloff`, `SoundSize`) e leitura dos 5 Built-in 3D Parameters.
     - **Módulo 3 — Estação 3 (`03A` Timeline, `03B` Layers, `03C` Unity):** música adaptativa de combate na Timeline (`120 BPM`, Markers, `Loop Region`, `Transition Region` com crossfade `1,4 s`/`1,6 s`, `AHDSR`, camada `Tension`), construção equivalente na visão `Layers` (`+ Stem Layer`, `Opacity`, `MaskRangeCanvas`, `Fade`, `MaskTransitionCard` e cursor vivo `AGUARDANDO -> TOCANDO`) e integração C# com `CombatIntensity` + `seek_speed`.
     - **Módulo 4 — Estação 4 (`04A` Studio, `04B` Unity):** ambiência estocástica com `Scatterer Instrument` (`async`, polifonia `4`, intervalo `220..620 ms`, dispersão `1.5..9.0 m`) e parada suave com `OaeStopMode.AllowFadeout`.
     - **Módulo 5 — Estação 5 (`05A` Studio DSP & Snapshots, `05B` Studio Banco VO & Programmer, `05C` Unity):** comparação prática das duas técnicas de **ducking** (**Sidechain Compressor** dinâmico `bus:/vo -> bus:/music` vs. **Snapshot** `vo_ducking` prioridade `220`), `Reverb` e `Lowpass`, empilhamento de snapshots (`180` vs `220`), `AdjustmentLayerCard` em `Layers`, `playground_vo_project.json` (`audio_table` + `Programmer Instrument` + `DialogueLine`) e carga sob demanda (`LoadBankFile`/`Unload`) sem `Release()` prematuro.
     - **Módulo 6 — Mixagem ao Vivo e UI (`06A` Studio + Unity Live-Linking, `06B` Unity VCAs & UI 2D):** bridge TCP `7450` ao vivo (Mixer e `Layers`), controle de VCAs (`GetVca`) e `ui_click` 2D fora do reverb.
   - Arquivos JSON de cues compatíveis simultaneamente com Node.js e `UnityEngine.JsonUtility` em `docs/tutorials/playground/cues/*.json`.

2. **Camada de Destaque Visual e Ferramenta MCP no OpenAudio Studio (`studio/Source/`, `scripts/`)**:
   - Componente não-interativo `TutorialHighlightOverlay` em `MainComponent`, posicionado sempre acima dos painéis e splitters dinâmicos sem interceptar eventos de mouse/teclado, desenhando máscara de escurecimento (*spotlight*), contorno com tokens de `Theme.h` e rótulo opcional sobre qualquer `component_id`.
   - Nova ferramenta MCP `studio_ui_highlight` em `McpProtocol.h` e `McpServer` (13ª ferramenta do catálogo, executada na Message Thread e visível em janela real e `studio_ui_screenshot` de janela completa).
   - Executor CLI `scripts/tutorial_studio_cue.js` que lê os cues JSON e avança passo a passo (ou em sequência temporizada) via servidor MCP TCP `127.0.0.1:7451`, além de `--validate` e `--dry-run`.

3. **Diretor Nativo de Tutorial e Câmera Suave na Unity 6 (`unity/com.openaudio.core/`)**:
   - Janela Editor-only `PlaygroundTutorialDirectorWindow` (`OpenAudio/Tutorial Director`) usando exclusivamente a API nativa `UnityEditor.Highlighter.Highlight("Inspector", ...)` e `Selection.activeGameObject` — sem instalar nenhum pacote novo no `manifest.json`.
   - API programática de transição e órbita suave no `AudioPlaygroundController` (`BeginCameraMoveCue`, `BeginCameraOrbitCue`, `CancelCameraCue`, `IsCameraCueActive`), preservando compilação dual (`#if UNITY_5_3_OR_NEWER` e `.NET 8` headless), passos automáticos sobre as superfícies e leitura contínua dos parâmetros 3D na órbita de 360° em torno de `Station2_Emitter3DTotem`.

4. **Script Gratuito de Pós-Produção com `ffmpeg` (`scripts/finalize_tutorial_video.ps1`)**:
   - Normalização de loudness em duas passadas para padrão YouTube (`-14 LUFS`, 48 kHz AAC 192 kbps) via `ffmpeg` local, com suporte opcional a recorte (`-Start`/`-End`), queima de legendas SRT (`-Subtitles`) e modo `-WhatIf`.

## Exclusões

- **Módulo 7 / Temporada 2 ("Receitas de Efeitos Sonoros"):** episódios dedicados a oclusão submersa, filtro de rádio/walkie-talkie, delay em Send/Return, carga com `Sustain Point` + `Key Off`, `Command Instrument` para slow-motion, eventos aninhados compostos e integração `OaeEventTrack`/`OaeEventClip` na Unity Timeline ficam explicitamente adiados para a Temporada 2 (fora do escopo desta change).
- Não versionar arquivos binários de vídeo (`.mp4`, `.mkv`, `.mov`) no repositório Git.
- Não adicionar `com.unity.learn.iet-framework`, `com.unity.recorder`, `Cinemachine` nem plugins comerciais da Unity Asset Store ao `unity/OpenAudioUnity/Packages/manifest.json`.
- Não alterar a ABI C (`native/include/oae_audio.h`), o formato binário `.bank` v1 nem a estrutura/conteúdo dos projetos `example_project/playground_project.json` e `example_project/playground_vo_project.json`.

## Capabilities

### New Capabilities
- `unity-bridge`: Formaliza (de forma complementar à change `add-unity-audio-playground`, ainda pendente de arquivamento) o diretor nativo de tutorial no Unity Editor (`PlaygroundTutorialDirectorWindow`), a API dual de movimentação e órbita suave de câmera no `AudioPlaygroundController` e o kit de 17 roteiros, cues JSON e pós-processamento gratuito em `docs/tutorials/playground/` e `scripts/`.

### Modified Capabilities
- `mcp-automation`: Adiciona a ferramenta `studio_ui_highlight` para ativar, configurar e limpar programaticamente o destaque visual de controles pela árvore semântica de `component_id` na Message Thread.
- `studio-shell`: Adiciona o overlay não-interativo `TutorialHighlightOverlay` sobre o `MainComponent` com preservação de z-order após redimensionamentos, alternância `Timeline`/`Layers` e recriação de splitters de docking.

## Impact

- **OpenAudio Studio (`studio/Source/UI/`, `studio/Source/MCP/`, `studio/Source/MainComponent.*`, `studio/Source/Main.cpp`)**: Novos arquivos `TutorialHighlightOverlay.h/.cpp`, integração em `MainComponent` e `McpServer`, nova entrada em `McpProtocol.h` e atualização da cobertura `--smoke-mcp` em `Main.cpp` (e conferência dos docs de protocolo caso citem contagem de ferramentas).
- **Unity Bridge (`unity/com.openaudio.core/Runtime/Components/AudioPlaygroundController.cs`, `unity/com.openaudio.core/Editor/PlaygroundTutorialDirectorWindow.cs`, `unity/com.openaudio.core/Tests/Runtime/AudioPlaygroundTests.cs`)**: Novos métodos de cue de câmera em `AudioPlaygroundController` mantendo paridade `.NET 8`, nova janela Editor isolada em `OpenAudio.Editor` e novos testes automatizados; `Packages/manifest.json` permanece inalterado.
- **Scripts (`scripts/tutorial_studio_cue.js`, `scripts/finalize_tutorial_video.ps1`)**: Utilitário Node.js cliente do MCP TCP `127.0.0.1:7451` e script PowerShell invocando `ffmpeg`.
- **Documentação (`docs/tutorials/playground/`)**: Novo diretório com `README.md`, 17 arquivos de roteiro (`00A`–`06B`) e cues JSON validáveis.
