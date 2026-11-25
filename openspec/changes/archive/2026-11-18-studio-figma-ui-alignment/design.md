# Design

## Context

Consulte [proposal.md](proposal.md) para motivação, escopo e exclusões e [specs/studio-shell/spec.md](specs/studio-shell/spec.md) para os requisitos observáveis.

A interface atual do OpenAudio Studio (`studio/Source/MainComponent.*`, `studio/Source/UI/`) apresenta quatro restrições estruturais que guiam este design:
1. **Hierarquia de docking e headers duplos**: `MainComponent::layout_workspace_components()` insere um `SlotHeaderBar` (`20 px`) acima de cada grupo que não seja barra de ferramentas, duplicando cabeçalhos sobre `InspectorPanel` (`+26 px`) e `ContextualBottomDeck` (`DockingLayoutManager::TabSlot` com `+28 px`).
2. **Contratos de automação MCP e smoke tests**: `McpServer.cpp` localiza alvos exclusivamente por `ComponentID` e aplica `dynamic_cast<juce::Button*>`, `juce::Slider*`, `juce::TextEditor*` e `juce::ComboBox*`. Os testes `--smoke-pro-ui`, `--smoke-timeline` e `--smoke-mcp` em `studio/Source/Main.cpp` dependem diretamente dos `ComponentID`s e dos comportamentos de `DockingLayoutManager`, `OaeSlider`, `GripHandle`, `TimelineParameterStrip`, `LayerStackView`, `InspectorPanel` e `GlobalParametersDeck`.
3. **Tokens parciais e hexadecimais hardcoded**: `Theme.h` já compartilha 19 tokens exatos com o Figma, mas ainda faltam tokens `tint*`, `fade*`, escalas tipográficas (`9/11/13 px`), raios (`4/5/10 px`) e borda `1.5 px`, enquanto `AuthoringTreeBrowser.cpp`, `MixerDeckPanel.cpp`, `DiagnosticsLogPanel` e `InspectorCardBase.cpp` ainda possuem literais `0xff...`.
4. **Fidelidade estrita aos dados do modelo**: `AuthoringModel`, `AuditionTransport` e `WaveformCache` não possuem multi-bank, quantização por clipe, fade assimétrico em segundos+curva na máscara, modos de mesclagem nem regras condicionais para Adjustment Layers.

## Goals / Non-Goals

**Goals:**
- Alinhar tokens, primitivas visuais, proporções, tipografia e layouts dos painéis principais ao protótipo do Figma (`HvAJmcrKKJY45OpBA83D4A`, `1:42` e `7:75`) usando apenas dados reais deriváveis.
- Eliminar cabeçalhos duplos em `AuthoringTreeBrowser`, `TimelinePanel`, `InspectorPanel` e `ContextualBottomDeck` sem quebrar drag-to-dock, divisores, janelas flutuantes ou persistência `oae_studio_layout.json`.
- Preservar `GripHandle`, minimização e flutuação de `ProjectToolbar` e `TransportBar` (`can_close = false`) e manter todos os recursos funcionais (`Key Off`, `Help`, `Log`, `Prj`, `Hide All`, `Dock`, `Del`, `Reset`, botão `...` dos cards do Inspector e `Lock`).
- Garantir compatibilidade contínua com `McpServer` e atualizar de forma coordenada os smokes de UI e o manual offline.

**Non-Goals:**
- Alterar `native/`, ABI C, compilador `oae_bankc`, formato `.bank`, pacote Unity ou esquema `project-v1`.
- Implementar multi-bank (`"3 Banks"`), quantização por clipe (`Next Bar`/`Next Beat`), fade em segundos+curva ou `Mesclagem` na máscara, botão `+ Adj. Layer` ou regras booleanas condicionais de Adjustment Layers (permanecem omitidos e fora de escopo para changes futuras).

## Decisions

### 1. Tokens (`Theme.h`), `OaeDarkLookAndFeel` e Primitivas Base

- **Extensão de `OaeTheme`**:
  - `Colors`: adicionar `tintCyan = 0x2900f5d4`, `tintGreen = 0x3838b000`, `tintAmber = 0x2effb703`, `tintPurple = 0x3db5179e`.
  - `TrackColors`: adicionar `fadeTint(juce::Colour c)` retornando `c.withAlpha(0.30f)` (`0x4d`) para cobrir as 8 cores de pista de forma uniforme.
  - `Typography`: adicionar `micro = 9.0f`, `compact = 11.0f`, `paramTitle = 13.0f` junto aos existentes `caption (10)`, `control (12)`, `body (14)`, `heading (16)`.
  - `Edges`: adicionar `cardCornerRadius = 4.0f`, `switcherCornerRadius = 5.0f`, `pillCornerRadius = 10.0f`, `selectedBorderStroke = 1.5f`.
  - `Sliders`: adicionar `largeTrackThickness = 6.0f` e `largeThumbDiameter = 14.0f`, mantendo os valores padrão para controles compactos.
- **Primitivas reutilizáveis (`studio/Source/UI/OaePrimitives.h/.cpp` ou arquivos dedicados em `UI/`)**:
  - `OaeToolbarButton` (`public juce::TextButton`) com enum `Variant { Neutral, AccentCyan, TintCyan, AccentPurple }`, altura padrão `24 px`, `rounded = 3 px`.
  - `OaeStatusBadge` (classe base visual + variante clicável herdando `juce::TextButton` para `Params (N)` e `FX`), `h = 20 px`, `rounded = 10 px`, variantes `{ Cyan, Green, Amber, Purple, Neutral }` com ponto `●` opcional.
  - `OaeSegmentedControl` (container `bgDark`, borda `borderActive`, `rounded = 5 px`, pílula ativa `tint*` + borda `accent*`, abrigando `juce::TextButton` filhos com `ComponentID`s individuais para compatibilidade com `StageViewSwitcher` e `BoolSegmented`).
  - `OaeValueBoxSlider` (`public OaeSlider`): subclasse de `OaeSlider` que desenha caixa `bgSurface` com borda `borderSubtle`, `rounded = 3 px`, altura `22 px` e texto `11 px` `textPrimary`, preservando arraste horizontal `1:1`, fator `0.1x` com `Shift`, reset por duplo-clique e `dynamic_cast<juce::Slider*>` no `McpServer`.
  - `OaeDarkLookAndFeel`: parametrizar cor do thumb (`juce::Slider::thumbColourId`), opção de trilha plana sem thumb (para o slider de opacidade na `LayerStackView`) e padronizar `ComboBox` / `TextEditor` para `bgSurface` + `borderSubtle` (`h = 22 px`).
  - **Alternativa considerada**: criar widgets customizados derivados diretamente de `juce::Component` para pills e caixas de valor. *Rejeitada* porque `McpServer` ignora componentes que não herdem de `juce::Button`/`juce::Slider`/`juce::ComboBox`/`juce::TextEditor`.
- **Higiene de cores**: migrar todos os literais `0xff...` em `AuthoringTreeBrowser.cpp`, `MixerDeckPanel.cpp`, `DiagnosticsLogPanel` (`MainComponent.cpp`) e `InspectorCardBase.cpp` para tokens de `OaeTheme`.

### 2. `WorkspaceBar` e Menu Overflow `☰`/`…` (`MainComponent.cpp`)

- `WorkspaceBar` passa a ter fundo `bgDark` (`#161719`), altura `28 px`, borda inferior `1 px` `borderSubtle`, `px = 12 px`.
- Lado esquerdo: rótulo `WORKSPACE` (`10 px` Bold `textSecondary`) e apenas **5** pills `WorkspacePillButton` (`20 px` alt., `tintCyan` + borda `1 px` `accentCyan` quando ativos, `bgSurfaceRaised` + `textMuted` quando inativos): `Trn`, `Brw`, `Stg`, `Ins`, `Dck` (preservando os `ComponentID`s `btn_vis_transport`, `btn_vis_browser`, `btn_vis_timeline`, `btn_vis_inspector`, `btn_vis_bottom_deck`).
- Lado direito: rótulo `Preset:` (`10 px` Medium `textMuted`), `combo_workspace_preset` (`h = 24 px`), `btn_preset_save` (`+ Save Preset`, `h = 24 px`) e um botão compacto `☰`/`…` (`btn_workspace_menu`, `24×24 px`) que abre um `juce::PopupMenu` contendo:
  1. Toggle `Diagnostics & Log (Log)` (aciona visibilidade de `DockPanelId::Diagnostics`);
  2. `Project Toolbar (Prj)` (alterna minimizado/foco de `DockPanelId::ProjectToolbar`);
  3. `Hide All` / `Restore` (`toggle_hide_all()`);
  4. Submenu `Dock Slots` (mesma lógica atual de `show_dock_menu()`);
  5. `Delete Custom Preset` (habilitado somente quando preset ativo não é built-in);
  6. `Reset Layout` (restaura `Default (Authoring)`).
- **Trade-off**: `--smoke-pro-ui` (`Main.cpp`) atualmente busca `btn_hide_all_panels` direto na árvore; será atualizado para validar `btn_workspace_menu` e os métodos públicos de `WorkspaceBar` (`toggle_hide_all()`, `toggle_panel(...)`, `delete_active_preset()`, `reset_layout()`).

### 3. `TopToolbarRow` (`ProjectToolbar`, `TransportBar`, `OaeMeterComponent`)

- **Preservação do docking de barras**: `DockPanelId::ProjectToolbar` e `DockPanelId::Transport` continuam encaixados lado a lado no topo (`Top` slot, sem `SlotHeaderBar`), com `GripHandle` (`12 px`), `can_close = false`, minimização por duplo-clique/menu e janelas flutuantes com `Dock Back`.
- **Ajuste visual de altura e superfície**: `kBarHeight` sobe de `34 px` para `36 px` sobre fundo contínuo `bgSurface` (`#202225`) com borda inferior `borderSubtle`.
- **`ProjectToolbar`**:
  - Mantém `gripHandle_` à esquerda, seguido da marca `OPENAUDIO` (`11 px` Bold `accentCyan`), botões textuais `OaeToolbarButton` (`New`, `Open`, `Save`, `Build Bank [F7]` na variante `AccentCyan`) e um botão discreto `Help` (`?`, `btn_help`, tooltip `"Help Manual (F1)"`, `24×24 px`).
  - `preferred_width()` é recalculado conforme as larguras reais dos botões textuais (`New`, `Open`, `Save`, `Build Bank [F7]`, `Help`), evitando que `ProjectToolbar` fique preso ao antigo `extent = 192` em `DockingLayoutManager::workspace_from_legacy` (atualizar `extent` inicial ou usar `projectToolbar_.preferred_width()` no topo semelhante a `transport_height`).
  - Em `set_minimized(true)`, oculta `New`, `Open` e `Help`, mantendo `OPENAUDIO`, `Save` e `Build Bank [F7]`.
- **`TransportBar` e `OaeMeterComponent`**:
  - Botões `Play`, `Pause`, `Stop`, `Loop` e `Key Off` passam a `24×24 px` com `VectorIconButton` em modo **contorno/tint** (`OutlineTint`): quando ativos, `Play` usa `accentGreen` + alpha verde, `Pause` usa `accentAmber` + alpha âmbar, `Loop` usa `accentCyan` + `tintCyan`; `Stop` e `Key Off` usam contorno neutro `borderActive` (com `Key Off` em formato compacto discreto ao lado de `Loop`).
  - `TransportReadout`: container `bgDark` com borda `borderSubtle`, `rounded = 3 px`, `h = 24 px`, texto `11 px` SemiBold `accentCyan`. Consulta o evento ativo no `AuthoringModel`: se `logic_items` possuir ao menos um item `"kind": "tempo"`, exibe `MM:SS.mmm  |  <bpm> BPM  |  <ts_upper>/<ts_lower>` via `TimelineSnap::get_active_tempo`; caso não exista marcador `tempo`, exibe apenas `MM:SS.mmm` (sem inventar `120 BPM | 4/4` fictício).
  - `statusLabel`: mantido discreto (`10 px` `textSecondary`, flex shrink/truncate) ao lado do readout, oculto quando minimizado ou muito estreito.
  - `paramsButton`: substituído/estilizado como `OaeStatusBadge` clicável (`btn_transport_params`, `h = 20 px`, `tintCyan`, borda/texto `accentCyan`, texto `"Params (N)"`).
  - `KSystemMeter`: `OaeMeterComponent` ganha modo `Orientation::HorizontalStereo` (desenhando duas barras L/R paralelas de `6 px` em `bgDark` com fundo de trilha `bgSurfaceRaised` e escala K-14 / dBFS com balística `BallisticMeter`). `MainComponent::timerCallback()` lê `engine_.get_bus_meter` em `bus:/` (quando inicializado e banco carregado) e alimenta `transportBar_.update_meter(peakL, peakR, rmsL, rmsR)`.
  - Em `set_minimized(true)`, `TransportBar` oculta `Key Off`, `statusLabel`, `paramsButton` e `KSystemMeter`.

### 4. Unificação de `DockHeader` (`SlotHeaderBar`) e `AuthoringTreeBrowser`

- **Cabeçalho único `DockHeader` (`SlotHeaderBar` evoluído para `24 px`)**:
  - `SlotHeaderBar` passa de `20 px` para `24 px` (`dockHeaderBg`, borda inf. `borderSubtle`).
  - Quando `panels.size() == 1`, a única `DockTabButton` (`btn_slot_tab_<key>`) exibe o rótulo de seção em caixa alta (`BROWSER`, `STAGE`, `PROPERTY INSPECTOR`) em `10 px` Bold `textSecondary`, preservando drag-to-dock sem fundo/borda de aba.
  - Lado direito quando `panels.size() == 1`:
    - Exibe metadado contextual opcional (`"1 Bank"` no Browser, `event_path_` ativo na Stage) em `10 px` Medium `textMuted`;
    - Para o `InspectorPanel` solo-encaixado, hospeda um componente acessório opcional ou espelha o botão `inspector_lock_button` (`🔓 Unlocked` / `🔒 Locked`) no próprio header de `24 px`, enquanto `InspectorPanel` oculta sua sub-barra interna duplicada quando encaixado solo (mantendo sua barra de `24 px` apenas quando flutuante, em grupo multi-aba ou instanciado isoladamente em testes).
    - Os botões de slot `<> ^ X` ficam ocultos visualmente em repouso no modo painel único e acessíveis via clique direito no `DockHeader` ou hover discreto (mantendo `btn_slot_<key>_move`, `_popout`, `_close` na árvore de componentes).
- **`AuthoringTreeBrowser`**:
  - Barra `BrowserTabs` (`34 px`, `p = 6 px`, `gap = 4 px`): 4 `DockTabButton`s (`22 px` alt., `rounded = 3 px`, borda `accentCyan` na aba ativa): `Events`, `Snapshots`, `Buses`, `Assets`.
  - Aba `Buses`: popula os barramentos (`bus:/...`) e abaixo uma pasta/seção dedicada `VCAs` (`vca:/...`) com suporte a criação (`New Bus` / `New VCA`) e seleção normal (`ItemType::Bus` / `ItemType::Vca`).
  - `SearchBox` (`26 px` alt., `bgDark`, borda `borderSubtle`, placeholder `"Search events, stems, tags..."`) sem botões `+`, `Dup`, `Del` na barra. Menu de contexto (clique direito sobre folha, pasta, cabeçalho ou área livre da árvore da aba) concentra `New ...`, `Import Audio...`, `Rename (F2)`, `Duplicate (Ctrl+D)` e `Delete (Del)`.
  - Itens da árvore: destaque selecionado com fundo `tintCyan`, borda `1 px` `accentCyan` e texto `accentCyan`; itens não selecionados em `textSecondary`.
  - Rodapé fixo `PROJECT PARAMETERS (N)` (`minHeight ~110 px` ou altura proporcional conforme número de parâmetros, máx. `35%` da altura): cabeçalho `10 px` Bold `textMuted` + lista rolável de `ParamTreeRow`s (`22 px` alt., `bgDark`, `rounded = 3 px`, nome em `accentCyan`, domínio real em `textMuted`: ex. `0.0 .. 1.0`, `0 .. 5` ou `Bool`/`Label`). Suporta clique esquerdo para selecionar `ItemType::GlobalParameter` no `InspectorPanel` e menu de contexto para CRUD de parâmetros globais.

### 5. Stage (`TimelinePanel`, `TimelineParameterStrip` / `LIVE PARAMS`, `LayerStackView`)

- **`StageModeHeader` (`38 px`, `bgSurface`, `px = 12 px`)**:
  - Esquerda: `breadcrumbBackButton_` (`30×24 px`) + texto do evento ou trilha clicável de breadcrumbs (`pai > filho` quando aninhado, `event:/...` quando raiz).
  - Centro geométrico absoluto (`x = centreX - switcherW/2`, `196×32 px`): `OaeSegmentedControl` (`Timeline | Layers`, `bgDark`, `borderActive`, `rounded = 5 px`, pílula ativa `tintCyan` + borda `accentCyan` + texto `accentCyan`, preservando os `ComponentID`s `timeline.mode.timeline` e `timeline.mode.layers`).
  - Direita: apenas no modo `Layers`, exibe `+ Stem Layer` (`OaeToolbarButton` variante `AccentCyan`, `timeline.layers.add_stem`). `+ Adj. Layer` fica omitido conforme Decisão 7.
  - **Proteção contra colisão em `1280×800`**: a coluna esquerda ocupa no máximo até `switcher.getX() - 8`, truncando com reticências (`true`) o texto do breadcrumb se o caminho for longo, garantindo zero sobreposição com o switcher centralizado.
- **Sub-barra exclusiva do modo `Timeline` (`TimelineToolbarRow`, `28–30 px`, `bgSurface`, borda inf. `borderSubtle`)**:
  - Visível somente quando `stage_view_mode_ == StageViewMode::Timeline` (`setVisible(!layersMode)`), alocando `Snap:`, `snapComboBox`, `Time / Bars`, `-`, `zoomSlider`, `+` e `playheadLabel`.
  - **Alternativa considerada**: compactar tudo à direita do switcher centralizado na barra de `38 px`. *Rejeitada* porque em `1280×800` sobram ~`256 px` à direita do switcher contra ~`474 px` necessários.
- **`TimelineParameterStrip` (`LIVE PARAMS`)**:
  - Alturas: `0 px` automático quando evento não possui parâmetros consumidos; `24 px` quando recolhido manualmente (`[>]`); `42 px` quando expandido (`[v]`).
  - Superfície `bgSurfaceRaised` (`#282b30`), rótulo/toggle `LIVE PARAMS` à esquerda (`btn_timeline_params_toggle` com `[v]`/`[>]`), e caixas `bgDark` (`rounded = 4 px`, `h = 26 px`) dentro do viewport horizontal com nome (`$name`), slider inline + leitura de valor ou `OaeStatusBadge` interativo para parâmetros `labeled` de 2 estados (ex.: `False/True`), mantendo `find_slider_by_name` funcional para parâmetros contínuos/discretos e `labeled` com ≥ 3 opções.
- **`LayerStackView`**:
  - `LayerColumnHeaders` (`h = 22 px`): `CAMADA (STEM & MIX)` (`w = 350 px`), `MÁSCARA DE ATIVAÇÃO (0.0 — 1.0)` e leitura alinhada à direita `Cursor ao vivo: $<param> = <val>` (quando houver máscara vinculada a parâmetro ativo).
  - `AdjustmentLayerCard` (`h = 46 px`, `cardHeaderBg`, borda `1 px` `accentPurple`): badge `FX` em `tintPurple`/`accentPurple`, título do efeito real existente e status real (`FX ATIVO`, `ATIVO · $<param>`, `BYPASSED`) — sem texto fake de regra condicional (`<= 0.25`).
  - `StemLayerCard` (`h = 92 px`, `cardBodyBg`/`bgSurface`, `rounded = 4 px`, borda `1.5 px` `accentCyan` contínua ao redor do card inteiro quando selecionado ou `1 px` `cardBorder` caso contrário):
    - Coluna esquerda (`w = 350 px`): barra colorida `4×18 px`, `EyeToggleButton` (`20×20`), botão `S` (`20×20`), título (`12 px` SemiBold), badge real `LOOP` / `1-SHOT` (`OaeStatusBadge` ou tag discreta, sem `Next Bar` fictício), miniatura de waveform (`bgDark`, `~188×30 px`) e controle plano `Opacidade XX%` (`4 px` track, sem textbox, arrastável/clicável 1:1 com Undo/Redo).
    - Coluna direita: seletor de parâmetro, controle real de `Fade: XX%` (`fade_norm`), `OaeStatusBadge` de estado (`AGUARDANDO`, `● TOCANDO (XX%)`, `● BASE ATIVA`, `MUTADO (0%)`, `CURVA MANUAL`) e `MaskRangeCanvas`.
  - `MaskRangeCanvas` (`h = 36 px`): substitui o trapézio anterior por barra horizontal plana de `10 px` sobre trilha `bgDark` (`rounded = 3 px`, `borderSubtle`), com blocos de fade translúcidos `OaeTheme::TrackColors::fadeTint(trackColour)` em `[min - fade, min]` e `[max, max + fade]`, bloco ativo sólido `trackColour` em `[min, max]`, texto inferior `Ativa: <min> – <max>` (`9 px` `textMuted`) e cursor vertical `2×32 px` `accentCyan`. Arraste nas bordas de `[min, max]` ou no corpo da faixa preserva o comportamento e Undo/Redo existentes.

### 6. `InspectorPanel` e Cards (`Inspector/*`)

- **Cabeçalho único (`24 px`)**: quando encaixado solo no slot, o título `PROPERTY INSPECTOR` e o botão `inspector_lock_button` integram-se no `DockHeader` (`24 px`) descrito na Decisão 4; quando flutuante, agrupado com outra aba de workspace ou isolado em teste unitário/smoke, `InspectorPanel` exibe seu próprio cabeçalho de `24 px` em `dockHeaderBg`.
- **Estilo Figma unificado nos dois modos (`Timeline` e `Layers`)**:
  - `InspectorCardBase`: corpo `cardBodyBg` (`#1a1d26`), borda `1 px` `cardBorder` (`#2c3140`), `rounded = 4 px`, padding interno `10 px`, espaçamento entre cards `10 px`.
  - `InspectorCardHeader` (`h = 28 px`, `cardHeaderBg` `#232733`): chevron colapsável, título `10 px` Bold `textPrimary`, badge textual direito em `accentCyan` (sem moldura pílula e sem faixa lateral de `3 px`) e botão `...` (`inspector_card_more_actions`) preservado à direita.
  - `InspectorKeyValueRow`: linhas `22 px` (`textSecondary` `11 px` à esquerda, controles `bgSurface` + `borderSubtle` `rounded = 3 px` à direita), adotando `OaeValueBoxSlider` para parâmetros numéricos.
  - `ClipCard`: quando `WaveformCache` possuir o `WaveformEnvelope` do sample selecionado, exibe subtítulo real de metadados (`48 kHz | Stereo | 15.0s`, acrescentando `N Bars` somente se houver marcador `tempo` no evento).
  - **Seção/Card contextual `MÁSCARA & TRANSIÇÃO` (`MaskTransitionCard`)**: exibida apenas quando há faixa/clipe selecionado em um evento e o palco está no modo `Layers` OU a faixa possui máscara ativa (`LayerMaskSpec::state != AlwaysActive`). Exibe parâmetro vinculado, limites `[min, max]` via `OaeValueBoxSlider`, `Fade (%)` normalizado real, `OaeStatusBadge` de estado e botão `Editar Curva na Timeline...`. Não exibe campos fictícios de duração em segundos/curva nem `Mesclagem`.
  - A ordem `ClipCard -> TrackCard -> EventHeaderCard` (e regras de colapso/criação) permanece intacta para compatibilidade com `--smoke-timeline` e `--smoke-authoring`.

### 7. `ContextualBottomDeck` e `GlobalParametersDeck`

- **Cabeçalho único `BottomDeckHeader` (`34 px`, `dockHeaderBg`)**:
  - Quando `BottomDeck` é o único painel em seu grupo do workspace (`group.tabs.size() == 1`), `MainComponent` omite o `SlotHeaderBar` externo e `TabSlot` renderiza diretamente o `BottomDeckHeader` (`34 px`), contendo à esquerda o rótulo arrastável `BOTTOM DECK` (`btn_slot_tab_bottom_deck` conectado a `drag_workspace_tab`/`drop_workspace_tab`), as 7 `DockTabButton`s (`22 px`, `rounded = 3 px`, aba ativa com borda `accentCyan` e texto `accentCyan`) e à direita `Pop-Out ^` (`btn_dock_pop_out`, `78×24 px`).
  - Nova ordem do `enum class DeckTab` e registro em `ContextualBottomDeck.cpp`:
    `Mixer (0)` → `Effect Chain (1)` → `Logic Conditions (2)` → `Playlist (3)` → `3D Spatializer (4)` → `Parameters (N) (5)` → `Diagnostics (6)`.
  - O rótulo da aba `Parameters (N)` atualiza dinamicamente conforme `global_parameters.size()`.
  - O antigo botão `Tabs` (`btn_dock_tabs_menu`) sai da barra e passa a abrir via clique direito sobre qualquer aba do header.
- **`GlobalParametersDeck` e `GlobalParamCard`**:
  - `popOutBtn` é removido da barra interna de `GlobalParametersDeck` (permanecendo `Pop-Out ^` apenas no `BottomDeckHeader`; quando flutuante em `OaeFloatingPanelWindow`, a própria janela flutuante fornece retorno via barra `Dock Back` ou fechamento da janela reintegrando à aba).
  - `ParametersFilterBar` (`34 px`, `bgSurface`): `GLOBAL PARAMETERS DECK`, chips `All (<total>)`, `• Used in Event (<used>)` (chip ativo em `tintCyan` + borda `accentCyan` + texto `accentCyan`), `Modified (<mod>)`, `ParamSearchInput` (`ed_params_search`, `bgDark`) e `Reset All` (`btn_reset_all_params`).
  - `GlobalParamCard` (`h = 131 px`, `cardBodyBg`, `1.5 px` `accentCyan` se selecionado/em foco, `1 px` `cardBorder` caso contrário): título `13 px` Bold `textPrimary`, subtítulo `10 px` `textSecondary`, `OaeStatusBadge` (`LIVE SEEK` cyan se `has_seek_speed()`, `TRUE`/`FALSE` para booleanos/2-estados, `DEFAULT` âmbar se no valor padrão, `MODIFIED` ciano se alterado), slider com trilha `6 px`/thumb `14 px` colorido + valor `13 px` Bold à direita (ou `OaeSegmentedControl` para `labeled` preservando `get_option_buttons()`), e rodapé `10 px` `textSecondary` listando alvos reais modulados no evento ativo e modificadores (`Velocity`, `Hold ativo`). Reset individual acessível via menu de contexto do card e duplo-clique no slider.

## Risks / Trade-offs

- **[Quebra de asserções estruturais em `Main.cpp`]** (`--smoke-pro-ui` valida `preferred_height() == 34/24`, `btn_hide_all_panels`, `paramsDeck.popOutBtn`, ordem de abas) → **Mitigação**: atualizar esses testes junto à implementação de cada etapa mantendo 100% da cobertura comportamental e preservando todos os `ComponentID`s usados por `McpServer`.
- **[Sobreposição do `StageModeHeader` centralizado em larguras <= `1280 px`]** → **Mitigação**: calcular a caixa do switcher centralizado primeiro e restringir as larguras de breadcrumb à esquerda e `StageActions` à direita com truncamento seguro no texto do caminho.
- **[Complexidade de supressão condicional do `SlotHeaderBar` no `BottomDeck` e `InspectorPanel`]** → **Mitigação**: aplicar a supressão exclusivamente quando `group.tabs.size() == 1 && !floating`, mantendo o fallback automático para `SlotHeaderBar` normal caso o usuário agrupe outro painel em abas no mesmo slot.
- **[Desalinhamento do slider de opacidade sem textbox]** → **Mitigação**: usar `OaeSlider` (`LinearHorizontal`, `NoTextBox`) com modo visual de barra plana em `OaeDarkLookAndFeel`, preservando `Shift` fino (`0.1x`), duplo-clique reset (`100%`) e `ComponentID` `layers.card.<i >.opacity`.
