# Design Técnico: Audio Playground 3D na Unity

## Context
O projeto precisa de uma experiência espacial em primeira pessoa com exercício visual e auditivo integrado de pisos por superfície, cones direcionais 3D, `Scatterer Instrument`, `Programmer Instrument` com locução falada e transições adaptativas em ambiente 3D.

A ponte Unity (`unity/com.openaudio.core`) e o motor nativo (`OpenAudioEngine`) já implementam todas as primitivas necessárias (`StudioListener`, `StudioEventEmitter`, `StudioBankLoader`, `EventInstance`, `Bank`, `OaeSystem`, Live-Linking TCP na porta `7450`). Este design define como construir a cena `AudioPlayground` e seu pipeline de assets de forma autocontida, reproduzível em CI e testável tanto no runtime `.NET 8` headless quanto no Unity Editor.

## Goals / Non-Goals
- **Goals**:
  - Aquisição 100% autônoma de assets WAV 48 kHz 16-bit PCM (download CC0 + normalização `ffmpeg` + geração de locuções via `System.Speech.Synthesis.SpeechSynthesizer` + fallback procedural determinístico offline).
  - Construção procedural da sala 3D e das 6 estações em `AudioPlaygroundController.cs` usando primitivas nativas da Unity, sem assets proprietários ou pacotes externos.
  - Arquitetura dual (`#if UNITY_5_3_OR_NEWER` e compilação headless `.NET 8`) permitindo cobertura completa de testes em `unity_test.ps1 -DotnetOnly` e Unity PlayMode.
  - Validação interativa em primeira pessoa (`WASD` + mouse look + `StudioListener`) cobrindo as 6 estações e o Live-Linking com o OpenAudio Studio.
- **Non-Goals**:
  - Alterar a ABI C (`native/include/oae_audio.h`), o formato binário `.bank` v1 ou a interface do OpenAudio Studio.
  - Introduzir dependências de URP/HDRP, ProBuilder, Asset Store ou arquivos `.fbx`.

## Decisions

### 1. Pipeline Autônomo de Assets e Bancos (`scripts/create_playground_assets.ps1`)
- **Aquisição e Normalização**:
  - O script tenta baixar amostras CC0 reais (passos em madeira/concreto/grama, fogueira/rádio, pássaros/vento e bases musicais) quando há conectividade e `ffmpeg` disponível, padronizando cada áudio com `ffmpeg -y -i <in> -ar 48000 -ac 1 -sample_fmt s16 <out.wav>`.
  - Para os diálogos do `Programmer Instrument` (`audio_table`), utiliza `System.Speech.Synthesis.SpeechSynthesizer` (SAPI/.NET Windows) para gerar arquivos `.wav` falados legíveis (normalizados para 48 kHz 16-bit PCM).
  - Caso o download ou ferramenta externa falhe/esteja indisponível (como em ambientes offline/CI isolados), aciona automaticamente um gerador procedural determinístico em PowerShell, gravando WAVs PCM 48 kHz 16-bit válidos para que o pipeline nunca falhe.
- **Compilação de Bancos**:
  - Gera `example_project/playground_project.json` (eventos, buses, VCAs, snapshot, parâmetros, `Scatterer`, `Multi`, `Single`, timeline adaptativa) e `example_project/playground_vo_project.json` (`audio_table` + `Programmer Instrument`).
  - Invoca `oae_bankc` para compilar `playground.bank` + `playground.bank.catalog.json` e `playground_vo.bank` + `playground_vo.bank.catalog.json`, copiando-os para `unity/OpenAudioUnity/Assets/StreamingAssets/`.

### 2. Arquitetura Dual Headless/.NET e Unity Runtime (`AudioPlaygroundController.cs`)
- `AudioPlaygroundController` separa a máquina de estado e chamadas à API `OpenAudio` da camada de renderização/input da Unity por `#if UNITY_5_3_OR_NEWER`.
- **API pública testável em `.NET 8` e Unity**:
  - `Initialize(OaeSystem system)`
  - `SetPlayerTransform(float px, float py, float pz, float fx, float fy, float fz)`
  - `SelectSurfaceAndStep(PlaygroundSurface surface)` (`Wood`, `Concrete`, `Grass`)
  - `UpdateEmitter3D(float distanceOverrideOrZero)` / atualização contínua de atributos 3D
  - `SetCombatZoneActive(bool active)`
  - `SetForestZoneActive(bool active)`
  - `SetCaveZoneActive(bool active)`
  - `TriggerProgrammerDialogue(string audioTableKey)`
  - `SetVcaLevel(string vcaKind, float linearLevel)` (`Master`, `Music`, `Sfx`)
  - `TriggerUiClick()`
- **Geração Procedural da Cena na Unity**:
  - No `Start()`, caso os objetos da sala ainda não existam na hierarquia, o controlador cria automaticamente os pisos das 3 superfícies, o totem 3D com cone visual direcional, os triggers das zonas de Arena, Bosque e Túnel Acústico, o totem interativo de diálogo (`[E]`) e o jogador FPS (`CharacterController`/transform + `Camera` + `StudioListener`).
  - Um `OnGUI()` leve exibe a legenda da estação atual, valores dos Built-in 3D Parameters (`Distance`, `Direction`, `Elevation`, `EventConeAngle`, `Speed`), sliders dos 3 VCAs, botão de SFX 2D, botão para carregar/disparar `playground_vo.bank` e status de conexão Live-Linking (`127.0.0.1:7450`).

### 3. Topologia da Sala 3D e Roteamento de Áudio

```text
+-----------------------------------------------------------------------------+
|                    AUDIO PLAYGROUND - TOPOLOGIA DA SALA                     |
|                                                                             |
|  [Estacao 1: Passos]       [Estacao 2: Emissor 3D]   [Estacao 3: Arena]     |
|  Pisos: Madeira /          Totem 3D + Cone           Trigger Area           |
|  Concreto / Grama          Roll-off + Built-ins      CombatIntensity 0<->1  |
|                                                                             |
|                     (o) Jogador FPS + StudioListener                        |
|                                                                             |
|  [Estacao 4: Bosque]       [Estacao 5: Tunel + VO]   [Estacao 6: HUD 2D]    |
|  Scatterer Estocastico     Snapshot cave_reverb +    Sliders VCA + UI SFX   |
|  Polifonia & Stealing      playground_vo.bank (VO)   Live-Linking Status    |
+-----------------------------------------------------------------------------+

                      ROTEAMENTO DE BUSES, VCAS E SNAPSHOT
                      ====================================

  event:/playground/footstep --------+
  event:/playground/emitter_3d ------+--> [bus:/sfx] ------> [vca:/playground_sfx] ----+
  event:/playground/ui_click --------+                                                 |
                                                                                       v
  event:/playground/adaptive_music -----> [bus:/music] ----> [vca:/playground_music] -> [bus:/]
                                            ^                                          ^   |
  event:/playground/forest_scatterer -------|-> [bus:/ambience] -----------------------+   |
                                            |                                              |
  event:/playground/dialogue_vo --------> [bus:/vo] (ducking) -----------------------------+
                                            |                                              |
  snapshot:/playground/cave_reverb ---------+----------------------------------------------+
                                            v
                                  [vca:/playground_master]
```

## Risks / Trade-offs
- **[Risco] Indisponibilidade de rede, rate-limit ou ausência de `ffmpeg`/vozes TTS em máquinas de CI** -> **Mitigação:** `scripts/create_playground_assets.ps1` implementa cadeia de fallback em três níveis (download CC0 + `ffmpeg` -> `SpeechSynthesizer` SAPI / síntese PCM nativa em PowerShell) produzindo sempre arquivos WAV PCM 48 kHz 16-bit válidos e determinísticos.
- **[Risco] Drift entre a cena `.unity` serializada e a estrutura gerada em código** -> **Mitigação:** Manter `AudioPlayground.unity` enxuta (apenas `OaeAudioManager`, `StudioBankLoader`, `OaeDiagnosticsOverlay`, iluminação direcional e `AudioPlaygroundController`), delegando a geometria das estações à construção determinística no runtime do controlador.
- **[Risco] Conflito de execução automática de áudio no Unity BatchMode/PlayMode** -> **Mitigação:** Expor método `Initialize(OaeSystem)` e flags de inicialização compatíveis com o ambiente de testes de `V1Audio2DTests.cs` / PlayMode sem depender de foco de mouse bloqueado (`CursorLockMode`).
