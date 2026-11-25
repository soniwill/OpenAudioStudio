# Proposal

## Why

O protótipo de referência do OpenAudio Studio no Figma (`HvAJmcrKKJY45OpBA83D4A`, frame `1:42` `"OpenAudioStudio — Stage Layers View"` e biblioteca `"OAE Components"`) estabeleceu uma linguagem visual coesa para a DAW — com tokens translúcidos (`tint*`), cabeçalhos de dock únicos em `dockHeaderBg`, caixas de leitura compactas no inspetor (`ValueBox`), barras de máscara planas com rampas de fade na `LayerStackView` e um `BottomDeckHeader` unificado — enquanto a implementação atual em JUCE acumula cabeçalhos duplos, cores hexadecimais hardcoded em painéis específicos, botões redundantes e divergências de ordem, tipografia e hierarquia visual.

Alinhar toda a interface do Studio a esse sistema visual e estrutural reduz ruído visual, elimina cabeçalhos duplicados, preserva integralmente os fluxos funcionais e os contratos de docking/MCP existentes e restringe rigorosamente o escopo aos dados reais já existentes no modelo e runtime, sem introduzir dados fictícios ou novas capacidades de engine.

## What Changes

- **Tokens e Primitivas Visuais (`Theme.h`, `OaeDarkLookAndFeel`, novos widgets base)**:
  - Adição dos tokens translúcidos `tintCyan`, `tintGreen`, `tintAmber`, `tintPurple` em `OaeTheme::Colors`, helper `OaeTheme::TrackColors::fadeTint`, novos níveis tipográficos (`9/10/11/12/13 px`), raios (`4/5/10 px`) e borda de seleção (`1.5 px`), além de eliminação de cores hexadecimais hardcoded em `AuthoringTreeBrowser.cpp`, `MixerDeckPanel.cpp`, `MainComponent.cpp` (`DiagnosticsLogPanel`) e `InspectorCardBase.cpp`.
  - Criação das primitivas reutilizáveis `OaeToolbarButton`, `OaeStatusBadge`, `OaeSegmentedControl` e `OaeValueBoxSlider` (subclasses padrão `juce::Button` / `juce::Slider` compatíveis com `McpServer` e smoke tests) e atualização de `DockTabButton` (`rounded=3 px`, borda `accentCyan` na aba ativa) e `OaeDarkLookAndFeel` (suporte a trilho `6 px`/thumb `14 px` colorido, `ComboBox`/`TextEditor` padrão `ValueBox` e barra de opacidade plana).
- **WorkspaceBar (Decisão 1)**:
  - Fundo `bgDark` (`28 px`), rótulo `WORKSPACE`, apenas os 5 `WorkspacePill` (`Trn`, `Brw`, `Stg`, `Ins`, `Dck`) em `20 px` (`tintCyan` + borda `accentCyan` quando ativos), rótulo `Preset:`, dropdown de presets, `+ Save Preset` e um menu compacto `☰`/`…` (`btn_workspace_menu`) agrupando `Log`, `Prj`, `Hide All / Restore`, `Dock`, `Del` e `Reset`.
- **TopToolbarRow (`ProjectToolbar` + `TransportBar`) (Decisão 2)**:
  - Preservação integral de `GripHandle`, minimização e flutuação (`DockPanelId::ProjectToolbar` e `DockPanelId::Transport`, `can_close = false`), atualizando altura útil (`36 px` nominal) e estilo visual interno: `ProjectToolbar` com marca `OPENAUDIO`, botões textuais (`New`, `Open`, `Save`, `Build Bank [F7]`) e `Help (F1)` discreto; `TransportBar` com botões `24×24` contorno/tint (`Play`, `Pause`, `Stop`, `Loop`) e `Key Off` discreto, `TransportReadout` em `bgDark` com timecode e `BPM | compasso` reais deriváveis quando houver marcadores `tempo` no evento, `statusLabel` discreto, pill `Params (N)` (`OaeStatusBadge`) e `KSystemMeter` horizontal estéreo L/R ligado a `bus:/`.
- **AuthoringTreeBrowser (Decisão 3)**:
  - Cabeçalho único `DockHeader` (`24 px`, `BROWSER` à esquerda, contagem real de banco e itens à direita — sem `"3 Banks"` fictício), barra `BrowserTabs` (`34 px`) com 4 abas `[ Events | Snapshots | Buses | Assets ]` (com `VCAs` agrupados dentro da aba `Buses`), `SearchBox` em `bgDark` sem botões `+ / Dup / Del` na barra (ações movidas para menu de contexto e atalhos `F2`/`Ctrl+D`/`Del`), destaque de seleção em `tintCyan` + borda `accentCyan` e seção fixa inferior `PROJECT PARAMETERS (N)` com linhas `bgDark`.
- **Stage (`TimelinePanel`, `TimelineParameterStrip`, `LayerStackView`) (Decisões 4 e 8)**:
  - Cabeçalho único `DockHeader_STAGE` (`24 px`) quando único no slot, seguido de `StageModeHeader` (`38 px`) com breadcrumb `< event_path` à esquerda, `OaeSegmentedControl` centralizado `[ Timeline | Layers ]` e `+ Stem Layer` à direita no modo `Layers`.
  - Sub-barra contextual exclusiva do modo `Timeline` (`28–30 px`) logo abaixo do `StageModeHeader`, abrigando `Snap:`, `snapComboBox`, `Time / Bars`, `- / zoomSlider / +` e `playheadLabel`.
  - Evolução da `TimelineParameterStrip` para a faixa `LIVE PARAMS` (`42 px` expandida, `24 px` recolhida via `[v]`/`[>]`, `0 px` automática quando o evento não consome parâmetros), fundo `bgSurfaceRaised` e pílulas `bgDark` com slider inline e `OaeStatusBadge` para parâmetros booleanos/2-estados.
  - Alinhamento da `LayerStackView`: cabeçalho de colunas (`CAMADA (STEM & MIX)` / `MÁSCARA DE ATIVAÇÃO (0.0 — 1.0)` / `Cursor ao vivo`), cards `StemLayerCard` (`92 px`) com borda inteira `1.5 px` `accentCyan` quando selecionado, barra de cor `4×18`, `Eye` e `S` `20×20`, badge real `LOOP`/`1-SHOT`, waveform em `bgDark`, barra de opacidade plana sem textbox, `Fade: XX%` real, `OaeStatusBadge` de status e `MaskRangeCanvas` com barra plana horizontal (`10 px`), rampas translúcidas `fadeTint` nas extremidades, legenda `Ativa: X – Y` e cursor vertical `accentCyan` (substituindo o trapézio anterior).
- **InspectorPanel e Cards (Decisão 5)**:
  - Eliminação do cabeçalho duplo unificando `PROPERTY INSPECTOR` e o cadeado `Lock` no `DockHeader` (`24 px`) quando encaixado solo; estilo Figma nos cards (`cardBodyBg`, `cardHeaderBg` `28 px`, `cardBorder`, linhas `InspectorKeyValueRow` / `ValueBox`) em **ambos** os modos (`Timeline` e `Layers`), preservando o botão `...` (`Reset / Copy / Paste`) e a hierarquia `Clip → Track → Event`.
  - Inclusão da seção/card recolhível `MÁSCARA & TRANSIÇÃO` exibida somente quando relevante (faixa/clipe de evento no modo `Layers` ou faixa com máscara ativa), exibindo metadados reais de sample quando disponíveis em `WaveformCache`.
- **ContextualBottomDeck e GlobalParametersDeck (Decisão 6)**:
  - Fusão do cabeçalho do Bottom Deck em barra única de `34 px` (`BottomDeckHeader`, com as 7 abas na ordem do Figma: `Mixer`, `Effect Chain`, `Logic Conditions`, `Playlist`, `3D Spatializer`, `Parameters (N)`, `Diagnostics` e botão único `Pop-Out ^` à direita; ação `Tabs` movida para menu de contexto das abas).
  - Remoção do `Pop-Out ^` duplicado dentro de `GlobalParametersDeck`; atualização da barra de filtros (`34 px`, `GLOBAL PARAMETERS DECK`, chips `All (N)`, `• Used in Event (N)` ativo em `tintCyan`, `Modified (N)`, busca e `Reset All`) e dos cards (`~131 px`, `cardBodyBg`, subtítulo, `OaeStatusBadge`, slider `6 px`/`14 px` ou `BoolSegmented` e rodapé com alvos modulados e modificadores reais do evento ativo).
- **Exclusões (Out of Scope — Decisão 7)**:
  - Nenhuma invenção de dados fictícios ou alteração de modelo/runtime/ABI C/`.bank`/Unity: multi-bank (`"3 Banks"`), quantização por clipe (`Next Bar` / `Next Beat`), fade assimétrico em segundos+curva na máscara, modos de mesclagem (`Mesclagem: Normal (Aditivo)`), botão `+ Adj. Layer` e regras booleanas condicionais de Adjustment Layer (`$param <= limiar`) ficam **explicitamente omitidos na UI** e registrados como futuras changes fora de escopo.

## Capabilities

### New Capabilities

_(Nenhuma — a change alinha e evolui exclusivamente a interface desktop existente do Studio.)_

### Modified Capabilities

- `studio-shell`: Alinhamento visual e estrutural ao protótipo do Figma em `WorkspaceBar` (menu `☰`/`…`), `TopToolbarRow` (mantendo `GripHandle`, minimização e flutuação com novo visual `OPENAUDIO`, `24×24` contorno/tint, `TransportReadout` real e `KSystemMeter`), `AuthoringTreeBrowser` (abas `Events`, `Snapshots`, `Buses` com `VCAs`, `Assets`, menu de contexto e rodapé `PROJECT PARAMETERS`), `TimelinePanel` (`StageModeHeader` centralizado + sub-barra exclusiva do modo `Timeline` + `LIVE PARAMS` recolhível + `LayerStackView` com barra plana e rampas translúcidas), `InspectorPanel` (`DockHeader` único com `Lock`, estilo `ValueBox` nos dois modos, botão `...` e seção `MÁSCARA & TRANSIÇÃO` contextual), `ContextualBottomDeck`/`GlobalParametersDeck` (7 abas reordenadas, cabeçalho único `34 px`, `Pop-Out ^` único e cards atualizados) e documentação/manual offline correspondente.

## Impact

- **Camada Studio UI (`studio/Source/UI/`, `studio/Source/MainComponent.*`)**:
  - `Theme.h`, `OaeDarkLookAndFeel.*`, `DockTabButton.h`, novas primitivas (`OaeToolbarButton`, `OaeStatusBadge`, `OaeSegmentedControl`, `OaeValueBoxSlider`), `ProjectToolbar.*`, `TransportBar.*`, `OaeMeterComponent.*`, `AuthoringTreeBrowser.*`, `TimelinePanel.*`, `TimelineParameterStrip.*`, `LayerStackView.*`, `InspectorPanel.*`, `Inspector/InspectorCardHeader.*`, `Inspector/InspectorCardBase.*`, `Inspector/ClipCard.*`, `Inspector/TrackCard.*`, `Inspector/EventHeaderCard.*`, `ContextualBottomDeck.*`, `GlobalParametersDeck.*`, `MixerDeckPanel.cpp`, `DockingLayoutManager.*` e `studio/CMakeLists.txt`.
- **Camada Testes / Smokes (`studio/Source/Main.cpp`)**:
  - Atualização pontual de asserções de altura/controles/ordem de abas em `--smoke-pro-ui`, `--smoke-timeline` e `--smoke-mcp`, preservando a cobertura integral e os `ComponentID`s consumidos pelo `McpServer`.
- **Camada Docs (`docs/UI_GUIDELINES.md`, `docs/manual/`)**:
  - Atualização dos tokens e diretrizes em `docs/UI_GUIDELINES.md`, revisão dos capítulos afetados do manual offline (`01`, `03`, `04`, `05`, `06`, `07`, `12`) e regeneração de capturas reais via MCP (`studio_ui_screenshot`).
- **Camadas não afetadas**: `native/` (`oae_audio`, `oae_bankc`), ABI C, formato `.bank`, `unity/` e contratos de `AuthoringModel` além de consultas read-only deriváveis.
