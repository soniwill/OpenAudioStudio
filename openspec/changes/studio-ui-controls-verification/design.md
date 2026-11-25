# Design Técnico: Verificação Automatizada Ponta a Ponta de Controles de UI do Studio (`studio-ui-controls-verification`)

## Context

Consulte [proposal.md](proposal.md) para motivação, escopo e exclusões, e os delta specs em [studio-shell](specs/studio-shell/spec.md) e [dev-harness](specs/dev-harness/spec.md) para os requisitos normativos.

No estado atual do código (`studio/Source/`):
- **Áudio headless vs. dispositivo físico:** `StudioEngine::init_headless()` (`StudioEngine.h:18`, `StudioEngine.cpp:23-43`) inicializa com `OAE_INIT_NO_DEVICE` e habilita `StudioEngine::render(float*, uint32_t)` (float32 intercalado, 2 canais L/R a 48 kHz) e `get_bus_meter()`. Contudo, `System::render` (`native/src/core/system.cpp:498-500`) exige `headless_ == true` (retornando `OAE_ERR_DEVICE` se `init_device()` tiver sido usado), e `MainComponent::MainComponent()` (`MainComponent.cpp:654-739`) sempre tenta `engine_.init_device()` primeiro, carrega automaticamente `example_project/project.json`, tenta bind TCP `7450` no `LiveBridgeServer`, lê/grava `oae_studio_layout.json` no diretório do executável e inicia timers a 30 Hz.
- **Smokes e MCP atuais:** Os 11 smokes em `studio/Source/Main.cpp` executados em série por `scripts/build_studio.ps1` não cobrem a árvore completa de controles da UI, enquanto `--smoke-mcp` exerce ferramentas de domínio que contornam os widgets; além disso, `studio_trigger_control` (`McpServer.cpp`) chama `onClick()` diretamente sem checar visibilidade/habilitação/`hitTest`.
- **Identidade e opacidade:** Existem controles interativos sem `ComponentID` em `AuthoringTreeBrowser`, `TimelinePanel`, `TrackHeaderPanel`, `TimelineTrackLane`, `TimelineParameterStrip`, `MixerDeckPanel`, `PlaylistDeck`, `GlobalParametersDeck`, `DiagnosticsLogPanel` e em cards do `InspectorPanel`, além de colisões simultâneas (`inspector_card_more_actions`, `layers.adjustment.<idx>`, `ahdsr_curve_editor`, `automation_curve_editor`, `btn_slot_tab_bottom_deck`). Widgets customizados tratam mouse/teclado diretamente sem expor alvos.
- **Diálogos modais e janelas flutuantes:** Há 6 invocações de `FileChooser`/`AlertWindow` (sendo 2 `runModalLoop`/`showOkCancelBox` síncronas em `AuthoringTreeBrowser.cpp:500-535`) e 4 `PopupMenu::show()` síncronos (`AuthoringTreeBrowser.cpp:111,349,368` e `BusCard.cpp:99`). Janelas destacadas (`OaeFloatingPanelWindow` em `DockingLayoutManager.cpp:272-338`) são `juce::DocumentWindow` registradas no `Desktop`/`TopLevelWindow` fora dos filhos diretos de `MainComponent`.

## Goals / Non-Goals

**Goals:**
- Garantir descoberta automática de 100% dos controles interativos (padrão e customizados) na árvore do `MainComponent` e em `OaeFloatingPanelWindow` sob a regra "Zero Controle Órfão".
- Comprovar que toda interação passa por pre-flight físico realista (visível, habilitado, bounds válidos, sem clipping, sem oclusão no `hitTest`) e produz o efeito real esperado: mutação no `AuthoringModel` + `Undo`/`Redo`, estado de View/Docking, e variação objetiva de sinal PCM/RMS/espectro em áudio headless.
- Executar a suíte completa em um único processo headless em poucos segundos (`< 5 s` full; `< 1 s` com `--filter=`), sem alterar `oae_studio_layout.json` do usuário.

**Non-Goals:**
- Refatorar o wiring de lambdas `onClick`/`onValueChange` dos painéis nem introduzir `ApplicationCommandManager` (escopo reservado para change arquitetural separada).
- Reescrever o ciclo de reconstrução dinâmica de widgets em `refresh()`/`build_ui()`.
- Modificar ferramentas ou contratos do servidor MCP (`mcp-automation`).
- Testar cada folha individual de todos os `PopupMenu`s contextuais ou realizar diff visual de screenshots.

## Arquitetura da Suíte

```text
+-------------------------------------------------------------------------+
|   OpenAudio Studio.exe --test-ui-controls [--filter=..] [--audit-only]  |
+-------------------------------------------------------------------------+
                                    |
        +---------------------------+---------------------------+
        v                                                       v
+----------------------------------+    +---------------------------------+
|   Fixture Sintetico Determin.    |    |  MainComponent(Options)         |
|   - WAV 440Hz, 880Hz, 220+4000Hz |    |  - headlessAudio = true         |
|   - Buses, VCAs, Snapshots       |--->|  - layoutFile = temp_dir        |
|   - 6 tipos Instrument + Layers  |    |  - autoLoadExample = false      |
|   - FX (Lowpass/EQ3), 3D, Params |    |  - enableLiveBridge = false     |
+----------------------------------+    |  - enableTimers = false         |
                                        |  - UiTestDialogSeam ativo       |
                                        +----------------+----------------+
                                                         |
          +----------------------------------------------+
          |                                              |
          v                                              v
+----------------------------------+    +---------------------------------+
|   1. UiControlAuditor            |    |  2. Contratos por Painel        |
|   - Varre Matriz de Contextos    |    |  - Usa UiInteractionDriver:     |
|     (MainComponent +             |    |    pre-flight (visible/enabled/ |
|      TopLevelWindow pop-out)     |    |    bounds/clipping/hitTest) ->  |
|   - Exige ID unico (sem colisao) |    |    sintese Mouse/Key/Wheel/Text |
|   - Exige ITestableCustomControl |    |  - Verifica:                    |
|     para widgets customizados    |    |    [A] AuthoringModel+Undo/Redo |
|   - Bloqueia UNCOVERED           |    |    [B] View/Layout/Pop-Out      |
|                                  |    |    [C] HeadlessAudioProbe (PCM) |
+-----------------+----------------+    +----------------+----------------+
                  |                                      |
                  +------------------+-------------------+
                                     v
             +-----------------------------------------------+
             | Relatorio Deterministico (stdout + JSON opc.) |
             | PASS / FAIL / UNCOVERED + Exit Code (0 / !=0) |
             +-----------------------------------------------+
```

## Decisions

### 1. `MainComponentOptions` e Execução 100% Headless Determinística (`MainComponent.h/.cpp`)
- **Decisão:** Adicionar `struct MainComponentOptions` (com valores padrão idênticos ao comportamento desktop atual) e novo construtor `explicit MainComponent(MainComponentOptions options)` (delegando o construtor padrão `MainComponent()` para o comportamento original). Campos:
  - `bool headlessAudio = false` -> quando `true`, chama exclusivamente `engine_.init_headless()` (pulando `init_device()` para que `System::render()` não retorne `OAE_ERR_DEVICE`).
  - `std::filesystem::path layoutFile` -> quando não vazio, substitui `app_dir / "oae_studio_layout.json"` no construtor e no destrutor `~MainComponent()`.
  - `bool autoLoadExample = true` -> quando `false`, não carrega `example_project/project.json` no boot.
  - `bool enableLiveBridge = true` -> quando `false`, evita bind na porta TCP `7450`.
  - `bool enableTimers = true` -> quando `false`, não inicia o `startTimerHz(30)` de `MainComponent`, `MixerDeckPanel` e `GlobalParametersDeck`, evitando que o timer concorra com o passo determinístico de `engine_.update()` e `get_bus_meter()` na sonda acústica.
- **Alternativa considerada:** Variáveis de ambiente globais lidas dentro de `MainComponent`. Rejeitada por dificultar isolamento de múltiplos testes no mesmo processo e criar estado implícito frágil.

### 2. Mapa Canônico de `ComponentID`s (Preenchimento e Desambiguação sem Quebrar MCP)
Todos os IDs documentados em `docs/specs/mcp-server-guide.md` (`btn_*`, `mixer_fader_*`, `mixer_mute_*`, `mixer_solo_*`, `timeline_view`, `minimap_view`, `track_lane_<i>`, `deck_*`, `inspector_volume`, `inspector_pitch`, `inspector_pan`, `inspector_max_inst`, etc.) são preservados intactos. Lacunas e colisões identificadas no código passam a seguir a convenção abaixo:

| Painel / Arquivo | Controles anteriormente sem ID ou colididos | `ComponentID` atribuído / desambiguado |
| :--- | :--- | :--- |
| `AuthoringTreeBrowser` | `searchBox_`, `treeView_`, `paramsList_` | `browser_search_box`, `browser_tree_view`, `browser_params_list` |
| `TimelinePanel` | `snapComboBox`, `timeFormatButton`, `zoomOutButton`, `zoomSlider`, `zoomInButton`, `ruler_`, `trackHeaders_`, scrollbars | `timeline.snap_combo`, `timeline.time_format`, `timeline.zoom_out`, `timeline.zoom_slider`, `timeline.zoom_in`, `timeline.ruler`, `timeline.track_headers`, `timeline.h_scroll`, `timeline.v_scroll` |
| `TrackHeaderPanel` / `Item` | `addTrackButton`, `TrackHeaderItem`, `titleLabel`, `volumeSlider`, `panSlider`, `muteButton`, `soloButton`, `autoButton` | `timeline.add_track`, `track_header_<i>`, `track_header_<i>.title`, `track_header_<i>.volume`, `track_header_<i>.pan`, `track_header_<i>.mute`, `track_header_<i>.solo`, `track_header_<i>.auto` (mantendo `track_color_swatch_<i>`) |
| `TimelineTrackLane` | `ClipComponent`, `AutomationLaneComponent` | `track_lane_<t>.clip_<i>`, `track_lane_<t>.automation` |
| `TimelineParameterStrip` | container, `item.slider`, `item.boolBadge` | `timeline_params_strip`, `timeline_param.<scope>.<name>.slider`, `timeline_param.<scope>.<name>.badge` |
| `LayerStackView` | `AdjustmentLayerCard` colidindo `bus` vs `track` index 0 | `layers.adjustment.bus.<bus_idx>.<fx_idx>` / `.bypass` e `layers.adjustment.track.<t_idx>.<fx_idx>` / `.bypass` |
| `MixerDeckPanel` | `panSlider`, `snapshotButtons_` | `mixer_pan_master` / `mixer_pan_bus_<nome>` / `mixer_pan_vca_<nome>`, `mixer_snapshot_<nome_sanitizado>` |
| `PlaylistDeck` | `up_`, `down_` | `playlist_move_up`, `playlist_move_down` |
| `GlobalParametersDeck` | `GlobalParamCard`, `slider_`, `segmented_`, opções do segmentado, `resetBtn` | `deck.param.<name>.card`, `deck.param.<name>.slider`, `deck.param.<name>.segmented`, `deck.param.<name>.opt.<idx>`, `deck.param.<name>.reset` |
| `DiagnosticsLogPanel` | `logEditor` | `diagnostics_log_editor` |
| `SlotHeaderBar` vs `TabSlot` | colisão `btn_slot_tab_bottom_deck` | `btn_slot_header_tab_<panel>` em `SlotHeaderBar`, preservando `btn_slot_tab_<panel>` em `TabSlot` |
| `InspectorCardHeader` | `moreButton_` colidindo `inspector_card_more_actions` entre cards simultâneos | `<card_id>.header` e `<card_id>.more_actions` |
| `Inspector/*` (9 cards + base) | todos os combos/sliders/botões/editores dinâmicos e editores de curva (`AhdsrCurveEditor`, `AutomationCurveEditor`) | prefixo canônico do card e papel (ex.: `inspector.event.bus`, `inspector.track.add_inst`, `inspector.clip.offset`, `inspector.clip.loop`, `inspector.param.<idx>.kind`, `inspector.mod.<ctx>.<idx>.ahdsr`, `inspector.auto.<ctx>.<idx>.curve`), mantendo `inspector_volume` em `EventHeaderCard`/`BusCard`/`VcaCard` (mutuamente exclusivos) |

- **Nota de compatibilidade:** Como `--smoke-pro-ui` ou documentação antiga podem referenciar `inspector_card_more_actions`, verificar usos existentes no código durante a aplicação e ajustar o `--smoke-pro-ui` se ele buscar diretamente o ID antigo.

### 3. `ITestableCustomControl` e `UiTestDialogSeam` (`studio/Source/Testing/UiTestHooks.h`)
- **`ITestableCustomControl`**: interface pura mínima (sem dependência de código de teste no binário de produção além de retornar structs de descrição):
  ```cpp
  struct TestableTarget {
      std::string id;         // ex: "clip_0_0.resize_right", "point_1", "mask_min_handle"
      std::string role;       // "drag", "click", "double_click", "wheel", "row"
      juce::Rectangle<int> localBounds;
      bool enabled = true;
  };
  class ITestableCustomControl {
  public:
      virtual ~ITestableCustomControl() = default;
      virtual std::vector<TestableTarget> get_testable_targets() const = 0;
  };
  ```
  Implementada nos 12 widgets customizados (`ClipComponent`, `AutomationLaneComponent`, `TimelineRuler`, `TimelineOverviewMinimap`, `MaskRangeCanvas`, `EqPlot`, `Spatializer3DDeck`, `AhdsrCurveEditor`, `AutomationCurveEditor`, `ParametersListComponent`, `TrackHeaderItem`, `GripHandle`). Isso evita coordenadas mágicas frágeis nos testes e garante que se o layout de um handle ou linha mudar, o contrato continua acoplado à geometria real pintada pelo componente.
- **`UiTestDialogSeam`**: singleton leve (ponteiro/callback thread-local ou estático desativado por padrão, `nullptr` em uso desktop normal). Quando ativo nos pontos de `FileChooser` (`MainComponent.cpp:1060,1104`, `AuthoringTreeBrowser.cpp:575`), `AlertWindow` (`MainComponent.cpp:830`, `AuthoringTreeBrowser.cpp:500,526`) e nos 4 `PopupMenu::show()` síncronos (`AuthoringTreeBrowser.cpp:111,349,368`, `BusCard.cpp:99`), registra que o diálogo foi invocado com título/opções esperados e retorna a resposta simulada configurada pelo teste sem entrar em `runModalLoop()` ou abrir janelas do sistema operacional.

### 4. `UiInteractionDriver` (Interação Realista Anti-Falso-Positivo)
Antes de disparar qualquer interação em um `Component*` ou `TestableTarget`, o driver executa obrigatoriamente um pre-flight físico:
1. Confirma visibilidade recursiva até a raiz do `TopLevelWindow` / `MainComponent` (`isVisible()` em toda a cadeia de ancestrais).
2. Confirma `isEnabled()` no alvo.
3. Confirma `bounds` positivos (`w > 0 && h > 0`) e que o ponto de interação central não está recortado fora da área visível de nenhum ancestral (`clippedBounds`).
4. Confirma `hitTest` real chamando `root->getComponentAt(pointInRoot)`: o componente retornado precisa ser o próprio alvo ou um descendente dele (ou `PopupMenu`/filho esperado), falhando com `OCCLUDED_HITTEST` caso um painel, overlay opaco ou outro widget cubra o controle.
5. Só então sintetiza eventos reais JUCE (`mouseMove`, `mouseDown`, `mouseDrag` com `ModifierKeys::shiftModifier` quando aplicável, `mouseUp`, `mouseDoubleClick`, `mouseWheelMove`, `keyPressed`, `ComboBox::showPopup` / seleção no popup ou clique em item, entrada caractere a caractere em `TextEditor`), seguidos de dreno controlado da Message Thread (`MessageManager::getInstance()->runDispatchLoopUntil(15)` até esvaziar `callAsync` pendentes).

### 5. `UiControlAuditor` e Matriz Canônica de Contextos ("Zero Controle Órfão")
- **Matriz Canônica de Contextos:** Como controles no `InspectorPanel`, `LayerStackView` e `ContextualBottomDeck` só existem visíveis quando o contexto apropriado está ativo, o auditor percorre deterministicamente 7 estados principais sobre o projeto fixture:
  1. `Root / Default`: Event selecionado (Timeline mode) + Browser aba `Events` + Mixer deck.
  2. `Browser Tabs`: alterna `Snapshots`, `Buses` (cobrindo VCA), `Assets` + seleção na `ParametersListComponent`.
  3. `Track & Automation`: seleciona Track com faixa de automação expandida (`auto`).
  4. `Clips & Decks`: seleciona sequencialmente `Single` (c/ AHDSR & Parameter Automation), `Multi` (`PlaylistDeck`), `Scatterer`, `Event`, `Command`, `Programmer` (`audio_table`), além de testar abas `Effect Chain`, `Logic Conditions`, `3D Spatializer`, `Parameters (N)`, `Diagnostics`.
  5. `Layers Mode`: alterna para `Layers`, exibindo `StemLayerCard`, `MaskRangeCanvas`, `AdjustmentLayerCard` e `MaskTransitionCard`.
  6. `Inspector Entities`: seleciona `Bus`, `Parameter`, `Snapshot`, `VCA` e `Sample`.
  7. `Pop-Out Window`: destaca um deck/painel para `OaeFloatingPanelWindow` e audita ambos `MainComponent` e todas as janelas via `juce::TopLevelWindow::getNumTopLevelWindows()`.
- **Classificação de componentes e lista branca read-only:** Componentes puramente expositivos (`juce::Label` não editável, `OaeMeterComponent`, `timecode_display`, `status`, `diagnostics_log_editor`, barras de status) são excluídos da exigência de contrato interativo mediante lista explícita de papéis read-only. Qualquer componente interativo (`Button`, `Slider`, `ComboBox`, `TextEditor`, `TreeView`, `ListBox`, `Label` editável ou `ITestableCustomControl`) descoberto fora da lista branca passa por normalização de ID dinâmico (ex.: `track_header_1.mute` -> `track_header.*.mute`, `deck.param.Gain.slider` -> `deck.param.*.slider`) e precisa corresponder a um contrato registrado no mapa da suíte; caso contrário falha com `UNCOVERED`.
- **Códigos determinísticos de falha:** `MISSING_ID`, `DUPLICATE_ID`, `ZERO_BOUNDS`, `CLIPPED`, `OCCLUDED_HITTEST`, `UNCOVERED`, `MODEL_MISMATCH`, `UNDO_MISMATCH`, `VIEW_MISMATCH`, `AUDIO_MISMATCH`, `DIALOG_TIMEOUT`.

### 6. `HeadlessAudioProbe` e Projeto Fixture Sintético
- **Fixture determinístico em diretório temporário:** Gerado programaticamente no início da suíte (reutilizando padrão `write_sine_wav` de `Main.cpp:167-206`):
  - `tone_440.wav` (senoide pura 440 Hz, amplitude 0.5, 2 s, 48 kHz mono/estéreo) — energia previsível para transporte, ganho, mute, solo, pan e 3D.
  - `tone_880.wav` (senoide 880 Hz, amplitude 0.5) — segunda faixa/stem para provar Solo, Multi/Layers crossfade/opacity e Switch de stems.
  - `dualtone_220_4000.wav` (soma de 220 Hz + 4000 Hz, 0.25 cada) — usado com filtro `lowpass` e `eq3` para provar atenuação objetiva de alta frequência via razão Goertzel `E(4000 Hz) / E(220 Hz)` antes/depois do controle e restauração após `effect_bypass`.
  - O projeto fixture inclui Master + `bus:/sfx` + `bus:/music` + `vca:/master_vca` + `snapshot:/duck` + `audio_table` + parâmetros locais/globais (`Continuous`, `Discrete`, `Labeled`), efeitos (`lowpass`, `eq3`) e evento com modo Timeline e Layers + Spatializer 3D. Compilado uma única vez via `PreviewRebuilder::rebuild_synchronously()` no início do runner (~200–400 ms).
- **Medição L/R e Espectral:** `HeadlessAudioProbe::capture(frames = 4096)` executa em blocos de 256 frames chamando `engine.update(); engine.render(block, 256);` e computa: `rms_l`, `rms_r`, `rms_total_db`, `peak_db`, balanço `rms_l / rms_r`, razão Goertzel em `4000 Hz / 220 Hz`, além de consultar `engine.get_bus_meter()` para confirmar coerência dos meters. Tolerâncias conservadoras (ex.: `> -20 dB` no Play, `< -90 dB` no Stop/Mute, queda `>= 4 dB` no fader reduzido, razão L/R > 2.0 ao panear à esquerda) evitam flakiness mantendo garantia real de caminho de áudio.

### 7. Orçamento de Desempenho, CLI e Organização de Arquivos
- **Separação modular:** Em vez de expandir `studio/Source/Main.cpp` (já >5.600 linhas), a suíte vive em `studio/Source/Testing/`:
  - `UiTestHooks.h` (`ITestableCustomControl`, `UiTestDialogSeam`)
  - `UiInteractionDriver.h/.cpp`
  - `UiControlAuditor.h/.cpp`
  - `HeadlessAudioProbe.h/.cpp`
  - `UiControlTestFixture.h/.cpp`
  - `UiControlContracts.h/.cpp` (ou dividido por grupo: Shell/Browser, Transport/Mixer/Track, Timeline/Layers, Inspector/Decks)
  - `UiControlTestRunner.h/.cpp` (entrypoint chamado por `Main.cpp`)
  Todos registrados explicitamente em `studio/CMakeLists.txt` (que não usa `GLOB`).
- **Interface CLI:**
  - `--test-ui-controls`: executa auditoria + todos os contratos.
  - `--filter=<padrao>`: suporta glob simples (`*`, ex.: `inspector.*`, `mixer*`, `timeline.*`) executando apenas os contratos casados e restringindo a falha `UNCOVERED` ao subconjunto filtrado quando explícito.
  - `--audit-only`: executa apenas a varredura estrutural/geométrica/"Zero Controle Órfão" nos 7 contextos sem acionar todos os contratos acústicos.
  - `--report=<caminho.json>`: gera relatório estruturado com `duration_ms`, `contexts_audited`, `controls_discovered`, `passed`, `failed`, `uncovered` e lista detalhada de violações.
- **Scripts e Gates:** `scripts/test_studio_controls.ps1` localiza o binário compilado (`build/Studio_<Config>/...`), aceita `-Config Debug|Release`, `-Filter`, `-AuditOnly`, `-ReportPath`, `-Build` (desligado por padrão para iteração sub-segundo) e repassa o exit code. `scripts/build_studio.ps1` adiciona `Invoke-StudioSmoke '--test-ui-controls'` à lista de smokes do gate `-Layer studio`.

## Risks / Trade-offs

- **[Risco] Flakiness por atualizações assíncronas (`schedule_refresh()` / `callAsync`) entre interação e asserção** -> **Mitigação:** `enableTimers = false` no `MainComponentOptions`, drenagem explícita da `MessageManager` no `UiInteractionDriver` após cada gesto e uso do projeto fixture local determinístico sem I/O de rede.
- **[Risco] Travamento do processo caso algum novo caminho abra modal síncrono sem seam** -> **Mitigação:** cobrir os 6 pontos de `FileChooser`/`AlertWindow` e os 4 `PopupMenu::show()` síncronos via `UiTestDialogSeam` logo na tarefa 1.2, além de watchdog com timeout por contrato no runner e preferência futura por APIs assíncronas.
- **[Risco] Sobrescrita ou conflito com `--smoke-pro-ui` ou `--smoke-mcp` devido à desambiguação de IDs** -> **Mitigação:** preservar intactos todos os IDs listados em `docs/specs/mcp-server-guide.md` e ajustar nos smokes existentes apenas eventuais buscas por IDs colididos (como `inspector_card_more_actions`).
- **[Risco] Compilação repetida de `.bank` inflar o tempo total de execução** -> **Mitigação:** compilar o banco do fixture uma única vez na inicialização do `UiControlTestRunner`, mantendo o tempo total alvo abaixo de 5 segundos em Release e abaixo de 1 segundo em execuções filtradas.
- **[Risco] Custo de manutenção quando novos botões/sliders forem adicionados à UI** -> **Mitigação:** esse atrito é intencional e benigno — o erro `UNCOVERED` aponta imediatamente o novo `ComponentID` descoberto e o arquivo de contrato correspondente em `studio/Source/Testing/`, tornando o registro incremental e barato.
