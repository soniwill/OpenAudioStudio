# Roadmap de Evolução — OpenAudio Studio vs. FMOD Studio

Este documento concentra o backlog de referência do projeto para planejar futuras proposals via `/openspec-propose` ou `/openspec-explore`. O avanço automático segue suspenso ([AGENTS.md](../AGENTS.md)): cada item abaixo só é iniciado mediante autorização explícita.

**Convenção de status:** `[ ]` planejado · `[~]` em change ativa (`openspec/changes/`) · `[x]` concluído/arquivado (`openspec/changes/archive/`)

---

## 1. Estado Atual

### Changes OpenSpec (arquivadas)
- `[x]` **`studio-ui-timeline-and-decks-phase2`** ([studio-shell](../openspec/specs/studio-shell/spec.md)) — Tokens semânticos de clipes/curvas em `Theme.h`, trilha de breadcrumb pai->filho em `TimelinePanel`, curvas Bézier com gradiente *ribbon* e arraste direto com Undo/Redo único em `AutomationLaneComponent`, e micro-layout `FlexBox`/`Grid` em `TrackHeaderPanel`, `PlaylistDeck`, `LogicConditionsDeck`, `Spatializer3DDeck` e `EffectChainDeck` (`10/10` tarefas, arquivada em 2026-11-14).
- `[x]` **`studio-ui-design-system-and-docking`** ([studio-shell](../openspec/specs/studio-shell/spec.md)) — Diretrizes `docs/UI_GUIDELINES.md`, tokens `constexpr` em `Theme.h`, `OaeDarkLookAndFeel` vetorial (knobs, ajuste fino com `Shift`, reset por duplo-clique com Undo/Redo, `OaeMeterComponent` balístico), micro-layout `FlexBox`/`Grid` e docking com abas reordenáveis e janelas destacáveis (*pop-out* / *dock back*) (`13/13` tarefas, arquivada em 2026-11-14).
- `[x]` **`spatialization-3d-and-builtin-params`** ([spatialization](../openspec/specs/spatialization/spec.md), [parameters](../openspec/specs/parameters/spec.md), [studio-shell](../openspec/specs/studio-shell/spec.md)) — Atributos 3D de listener e `EventInstance`, cones direcionais, curvas de roll-off (`Linear`, `Inverse`, `LinearSquared`), envelopment (`Sound Size` e `Min Extent`), Built-in 3D Parameters automáticos (`Distance`, `Direction`, `Elevation`, `EventConeAngle`, `Speed`), evolução do `Spatializer3DDeck` e sincronização 3D no bridge Unity (`6/6` tarefas, arquivada em 2026-11-14).
- `[x]` **`parameters-sheets-and-modifiers`** ([parameters](../openspec/specs/parameters/spec.md), [instruments](../openspec/specs/instruments/spec.md), [studio-shell](../openspec/specs/studio-shell/spec.md)) — Parameter Sheets (`sheet_parameter`, `param_min`, `param_max` / chunk `PSHT`), parâmetros locais/globais completos (`Discrete`, `Labeled`, `Seek Speed`, `Velocity`, `Hold during playback` / chunk `PMOD`) e edição de `user_props` / `notes` / `tags` em eventos (`6/6` tarefas, arquivada em 2026-11-14).
- `[x]` **`timeline-transitions-runtime-parity`** ([spec canônico](../openspec/specs/timeline-logic/spec.md), [instruments](../openspec/specs/instruments/spec.md)) — Paridade em runtime de `TransitionMarker` / `TransitionRegion` para marcadores `Marker`, `crossfade_ms` em `sync_timeline_jump` e *Sound Stealing (`Oldest`)* no `Scatterer Instrument` (`6/6` tarefas, arquivada em 2026-11-14).
- `[x]` **`timeline-logic-ui`** ([spec canônico](../openspec/specs/timeline-logic/spec.md), [studio-shell](../openspec/specs/studio-shell/spec.md)) — Manipulação direta na `TimelineRuler`, `LogicConditionsDeck` e integridade referencial de `Marker` no `AuthoringModel` (`6/6` tarefas, arquivada em 2026-11-14).
- `[x]` **`async-instrument-authoring`** ([spec canônico](../openspec/specs/instruments/spec.md), [studio-shell](../openspec/specs/studio-shell/spec.md)) — Propriedade `async` em schema/`bankc`/`AuthoringModel` e toggle **Async** no `InspectorPanel` (`3/3` tarefas, arquivada em 2026-11-14).
- `[x]` **`instruments-playlist-nesting-ui`** ([spec canônico](../openspec/specs/instruments/spec.md), [studio-shell](../openspec/specs/studio-shell/spec.md)) — Playlist mista com `target_event` (`[EVT]`) no `PlaylistDeck` e `Programmer Instrument` + `audio_table` ponta a ponta (`bankc`, `.bank`, `AuthoringModel`, `InspectorPanel`, `TimelineTrackLane` `[PRG]`, `7/7` tarefas, arquivada).
- `[x]` **`instruments-scatterer-spawners`** ([spec canônico](../openspec/specs/instruments/spec.md), [studio-shell](../openspec/specs/studio-shell/spec.md)) — `Scatterer Instrument` com agendamento estocástico, polifonia, dispersão espacial e UI no Studio (`7/7` tarefas, arquivada).
- `[x]` **`timeline-nested-event-ux`** ([spec canônico](../openspec/specs/studio-shell/spec.md)) — Sub-lanes automáticas na sobreposição, badge `[EVT] <nome>`, waveform resumo do filho e duplo-clique para abrir o evento alvo (`6/6` tarefas, arquivada).
- `[x]` **`studio-command-type-ui`** ([spec canônico](../openspec/specs/studio-shell/spec.md)) — Seletor de `command_type` (`start_event`, `stop_event`, `snapshot_on`, `snapshot_off`) no Inspector e rótulo na timeline (`5/5` tarefas, arquivada).
- `[x]` **`instruments-runtime-parity`** ([spec canônico](../openspec/specs/instruments/spec.md)) — Execução em runtime de `Event` e `Command` instruments + cobertura CTest.
- `[x]` **`harness-foundation`** ([spec canônico](../openspec/specs/dev-harness/spec.md)) — Gate unificado (`scripts/verify.ps1`).

### Base já implementada no produto (Épicos 1–9 e 11)
- `[x]` **Motor e ABI C (`engine-core`, `c-api-abi`):** reprodução/streaming de sons, mixagem hierárquica, DSP, eventos, timeline, parâmetros, modulação, snapshots e VCAs.
- `[x]` **Publicação e Banco v1 (`bank-format`, `project-publish`):** parser JSON, compilador `oae_bankc`, catálogo, publicação atômica com recuperação e carregamento nativo.
- `[x]` **Studio e Unity (`studio-shell`, `unity-bridge`):** navegador, inspetor, timeline com waveforms, prévia em sincronia, mixer, deck contextual, manual offline, pacote Unity e `AudioPlayground` (3D / relógio DSP).
- `[x]` **MCP, Medição e Live-Linking (`mcp-automation`, `metering-profiling`, `live-update`):** servidor MCP local (TCP + tools `studio_*` + recursos `oae://`), medição Peak/RMS/K-scales/ITU-R BS.1770 (EBU R128) e Live-Linking Studio–Unity.
- `[x]` **Áudio Interativo Avançado (Épico 9):** sends/returns, mixagem por faixa, instrumentos assíncronos e `Programmer`, regiões de parâmetro, loops sobrepostos, pitch da timeline e bancos sob demanda.

---

## 2. O que ainda falta em relação à suíte FMOD Studio 2.04 (Paridade)

### Prioridade Alta — Instrumentos, Timeline e Modulação
- `[ ]` **`instruments-scatterer-dynamic-modulation`** (`instruments`, `modulation-automation`, `studio-shell`)
  - *O que falta:* Modulação dinâmica de parâmetros em spawners do `Scatterer Instrument` (explicitamente adiada da Fase 1).
- `[ ]` **`modulation-automation-ui`** (`modulation-automation`, `parameters`, `studio-shell`)
  - *O que falta:* Interface visual para moduladores **AHDSR**, **Random**, **Seek** e **LFO** no Inspector, além de editor de **Parameter Sheets** e curvas de automação multiponto por parâmetro (Épico 12 / `T10b`).
- `[ ]` **`modulation-parameters-runtime-parity`** (`modulation-automation`, `parameters`, `studio-shell`)
  - *O que falta (Etapa 2 pós-auditoria benchmark):* Moduladores dinâmicos LFO/Envelope contínuos em runtime, Seek Speed completo em parâmetros e automação contínua em parâmetros de DSP por faixa/barramento.

### Prioridade Média — Espacialização 3D, Mixing e Profiling
- `[x]` **`spatialization-3d-runtime`** ([spatialization](../openspec/specs/spatialization/spec.md), [parameters](../openspec/specs/parameters/spec.md)) — Concluído na change arquivada `spatialization-3d-and-builtin-params` (atributos 3D listener/`EventInstance`, cones direcionais, roll-off `Linear`/`Inverse`/`LinearSquared`, envelopment `Sound Size`/`Min Extent`, Built-in 3D Parameters `Distance`/`Direction`/`Elevation`/`EventConeAngle`/`Speed`, `Spatializer3DDeck` e bridge Unity). Testes multiplataforma Linux/macOS permanecem fora do escopo Windows atual.
- `[ ]` **`profiling-live-sessions`** (`metering-profiling`, `live-update`, `studio-shell`)
  - *O que falta:* **Profiler em tempo real** no Studio conectado ao jogo (CPU DSP %, contagem de vozes/instâncias ativas, consumo de memória de bancos e captura de linha do tempo de chamadas via Live-Linking).
- `[ ]` **`mixing-sidechain-and-snapshots`** (`mixing`, `snapshots-vcas`)
  - *O que falta:* **Sidechain** (compressor/ducking entre buses), **blending de snapshots** por prioridade/intensidade dinâmica e visualização em grafo da topologia de roteamento.

### Prioridade Base — Bancos, Assets e Hardening
- `[ ]` **`banks-assets-streaming-codecs`** (`bank-format`, `banks-assets`, `engine-core`)
  - *O que falta:* Compressão de áudio em `.bank` (ex.: Vorbis/Opus além de PCM), **streaming assíncrono em disco** por chunks e suporte a tabelas de localização/idiomas (locales).
- `[ ]` **`engine-handle-collision-safety`** (`engine-core`, `c-api-abi`)
  - *O que falta:* Prevenção de colisão de geração de handles após reutilizações além do limite de 24 bits (pendência registrada em [memory.md](ai/memory.md)).

---

## 3. Onde o OpenAudio Studio pode superar o FMOD Studio (Diferenciais)

- `[ ]` **`mcp-ai-assisted-authoring`** (`mcp-automation`, `studio-shell`)
  - *Base já existente:* servidor MCP local (`studio_*` e recursos `oae://`) + medição ITU-R BS.1770 / EBU R128.
  - *Próximo passo:* fluxos de autoria e mixagem assistidos por IA (presets inteligentes, auditoria automática de loudness e regressões acústicas automatizadas no projeto).
- `[x]` **`timeline-breadcrumb-and-preview-ux`** ([studio-shell](../openspec/specs/studio-shell/spec.md))
  - *Base já existente:* sub-lanes automáticas, waveform do filho e duplo-clique em `timeline-nested-event-ux`.
  - *Próximo passo:* navegação instantânea por **breadcrumb** entre eventos pais/filhos e inspeção acústica sem janelas modais pesadas.
- `[ ]` **`git-friendly-project-and-ci`** (`project-publish`, `dev-harness`)
  - *Base já existente:* projeto em JSON legível, publicação atômica de `.bank` e gate local `scripts/verify.ps1`.
  - *Próximo passo:* diff/merge semântico de projetos de áudio e pipelines de CI reproduzíveis sem dependência de tooling proprietário.

---

## Referências relacionadas
- [Índice de capacidades e legado](specs/README.md)
- [Memória operacional](ai/memory.md)
- [Blueprint consolidado (somente leitura)](legacy/blueprint.md)
