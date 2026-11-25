# Design

## Context

Este documento detalha a arquitetura técnica dos seis pilares definidos em `proposal.md` e especificados no delta de `specs/studio-shell/spec.md` para o OpenAudio Studio (`oae_studio`, JUCE 8). Todas as evoluções preservam o Design System central em `studio/Source/UI/Theme.h` (`OaeTheme`) e as regras de `docs/UI_GUIDELINES.md`.

## Goals / Non-Goals

**Goals:**
- Eliminar o atraso físico no arraste da bolinha de `OaeSlider` alinhando geometria do `LookAndFeel` e mapeamento `1:1` por extensão útil do trilho.
- Reorganizar o `InspectorPanel` em cards colapsáveis independentes por escopo (`Clip` -> `Track` -> `Event`) com subgrupos lógicos claros.
- Introduzir cores editáveis por pista no `AuthoringModel`, aplicá-las ao background dos clipes (com badge exclusivo para tipo de instrumento na barra de título) e destacar claramente pista selecionada e clipe selecionado no palco.
- Garantir renderização poligonal de waveforms em duas camadas (silhueta de pico em gradiente + núcleo RMS) para todos os tipos de instrumentos/eventos (`single`, `multi`, `scatterer`, `programmer`, `event`).
- Fornecer *ghost geometry* contínua durante o arraste de clipes na mesma pista ou entre pistas.
- Evoluir o `DockingLayoutManager` com `WorkspaceBar`, visibilidade independente/total de painéis, reconfiguração de slots, 3 templates nativos, criação/exclusão de layouts customizados e *Reset Layout*.
- Incluir validação visual determinística via `studio_ui_screenshot` do servidor MCP do Studio.

**Non-Goals:**
- Alterar o motor nativo C/C++, ABI C, compilador `oae_bankc`, formato `.bank` ou pacote Unity.
- Introduzir dependências externas fora do stack C++20 + JUCE 8 já utilizado por `oae_studio`.

## Decisions

### Decisão 1 — Matemática de Arraste 1:1 em `OaeSlider` e `OaeDarkLookAndFeel`

- **Problema raiz**: `OaeSlider::mouseDown` força `setSliderSnapsToMousePosition(false)` (o que faz o `juce::Slider` dividir o delta do mouse pelo `mouseDragSensitivity` fixo de `250 px`, causando sub-deslocamento em sliders de largura diferente de `250 px`), e `OaeDarkLookAndFeel` herda `getSliderThumbRadius` (`9 px`) do `LookAndFeel_V2` enquanto desenha um thumb de raio `6 px` (`thumbDiameter = 12.0f`).
- **Abordagem escolhida**:
  1. Em `OaeDarkLookAndFeel`, sobrescrever `getSliderThumbRadius(juce::Slider&)` retornando `OaeTheme::Sliders::thumbDiameter * 0.5f` (`6 px`) para sliders lineares horizontais/verticais, e sobrescrever `getSliderLayout(juce::Slider&)` garantindo que `sliderBounds` corresponda exatamente ao trilho útil descontando o raio do thumb e os limites do `textBox`.
  2. Em `OaeSlider`, manter `setSliderSnapsToMousePosition(false)` apenas para evitar salto quando o usuário clica no meio do trilho/thumb, mas no `mouseDown` calcular a extensão útil em pixels do trilho (`trackSpanPx`, via `getLookAndFeel().getSliderLayout(*this).sliderBounds` menos `2 * thumbRadius`) e ajustar dinamicamente a proporção de avanço: em velocidade normal (`1.0x`), cada pixel deslocado pelo mouse corresponde exatamente a `1.0 / trackSpanPx` da proporção normalizada do slider (`valueToProportionOfLength` / `proportionOfLengthToValue`), preservando escalas logarítmicas (`skewFactor`); quando `Shift` está pressionado, o incremento passa a `OaeTheme::Sliders::fineDragScale` (`0.1f`).
- **Alternativas consideradas**: Deixar `setSliderSnapsToMousePosition(true)` em sliders lineares e usar apenas `mouseDragSensitivity`. Descartada porque causaria salto repentino do valor ao clicar fora do centro exato da bolinha e não resolveria a divergência quando `Shift` é alternado no meio do arraste.

### Decisão 2 — Arquitetura Modular em Cards Colapsáveis (`InspectorSectionCard`) no `InspectorPanel`

- **Estrutura**: Substituir a construção monolítica de `InspectorPanel::create_event_editor` por um conjunto de cartões colapsáveis (`InspectorSectionCard`, encapsulando header clicável com chevron vetorial, barra lateral colorida por escopo/pista, badge de contexto e corpo organizado com `juce::FlexBox`/`juce::Grid`).
- **Hierarquia Contextual (Accordion)**:
  1. **Card `CLIP`** (exibido no topo quando `track_index >= 0 && instrument_index >= 0`): barra lateral com a cor da pista + badge de tipo (`[SGL]`, `[MLT]`, `[SCT]`, `[PRG]`, `[EVT]`, `[CMD]`) e nome do clipe. Subgrupos internos separados por divisores sutis:
     - *Source & Trigger*: sample/playlist summary/programmer_key/command_type+target, `Async`, `Loop`, `Start Offset`.
     - *Timing & Fades*: `Offset`, `Length`, `Fade In`, `Fade Out`, `Pitch`, `Volume`.
     - *Trigger Behavior / Spawner* (quando aplicável: `Scatterer`, `Multi`, `Probability`, `Trigger Conditions`).
     - *Clip Modulation*: lista de moduladores do clipe com `AhdsrCurveEditor` / `Random`.
  2. **Card `TRACK`** (exibido para a pista selecionada `track_index >= 0`, sem empilhar as outras pistas do evento): nome da pista, seletor rápido de cor da pista (swatches da paleta `OaeTheme`), `Volume`, `Pan`, `Mute/Solo` e resumo/atalho de automação da pista.
  3. **Card `EVENT`** (exibido na base): dividido em subgrupos (*Routing & Bus*, *Event Macros*, *Local Parameters*, *Event Modulation*, *Automation Curves* e *User Properties / Notes / Tags*).
- **Alternativas consideradas**: Usar apenas abas (`Event | Track | Clip`). O usuário escolheu explicitamente o formato de cards empilhados e colapsáveis (**Accordion**), que mantém visíveis simultaneamente os parâmetros de clipe, pista e evento sem esconder o contexto pai.
- **Preservação de estado**: `InspectorPanel` preserva o estado aberto/recolhido de cada card (`clipExpanded_`, `trackExpanded_`, `eventExpanded_`) e a posição de scroll do `juce::Viewport` durante atualizações do `AuthoringModel`.

### Decisão 3 — Paleta de Cores por Track, Destaque no Palco e `ClipComponent`

- **Persistência no Modelo (`AuthoringModel`)**:
  - Como o schema `project-v1` validado pelo compilador `oae_bankc` define `"additionalProperties": false` em `#/$defs/track`, a cor e o nome customizado de cada pista `i` são persistidos no objeto `user_props` do evento (`"track_<i>_color"` e `"track_<i>_name"`, no mesmo padrão de `"track_<i>_mute"` e `"track_<i>_solo"`), mantendo `tracks[i]` estritamente compatível com o compilador (com fallback de leitura para campos legados `"color"` / `"name"` em `tracks[i]`).
  - Ao criar uma nova pista ou carregar um evento antigo sem `"track_<i>_color"`, um helper determinístico (`OaeTheme::TrackColors::hexForIndex(i)`) atribui a cor cíclica `palette[track_index % paletteSize]` sem quebrar projetos legados nem afetar `oae_bankc`.
  - Métodos no `AuthoringModel` permitem consultar e alterar a cor da pista com suporte a Undo/Redo.
- **Destaque de Pista Selecionada (`TrackHeaderItem` + `TimelineTrackLane`)**:
  - Quando `model_.selection().track_index == track_index_`, tanto o `TrackHeaderItem` quanto a `TimelineTrackLane` correspondente entram no estado selecionado: tarja lateral na cor da pista + `accentCyan`, fundo da raia no palco com tint translúcido (`trackColour.withAlpha(0.10f)`) e bordas contínuas superior e inferior (`1.5 px`) atravessando cabeçalho e palco para distinguir facilmente a pista ativa quando existem 2 ou mais pistas.
- **Estética do `ClipComponent`**:
  - **Background do corpo**: pintado diretamente com a cor da pista (`trackColour`), ajustando alpha/brilho entre não selecionado e selecionado.
  - **Barra de título superior (~18 px)**: abriga exclusivamente o **badge/ícone compacto do tipo de instrumento** (`[SGL]`, `[MLT]`, `[SCT]`, `[PRG]`, `[EVT]`, `[CMD]`) seguido do nome legível do clipe/alvo (estilo Ableton Live 12 / Logic Pro). O tipo não altera a cor de fundo do corpo do clipe.
  - **Estado selecionado do clipe**: barra de título iluminada/invertida em alto contraste (`trackColour.brighter(0.55f)` com texto escuro de alto contraste), corpo mais vivo, alças de redimensionamento/fade visíveis nas extremidades e moldura dupla (`1.0 px` interno escuro + `2.5 px` externo em `OaeTheme::Colors::accentCyan` / `indicator`).

### Decisão 4 — Resolução Universal de Waveform, Renderização Poligonal e Ghost Drag

- **Resolução Universal de Waveform**:
  - Extrair helper dedicado (ex.: `resolve_clip_waveform_sources(...)`) consultado por `ClipComponent`:
    - `single`: usa `"sample_id"` diretamente.
    - `multi` e `scatterer`: percorre `"playlist"` coletando entradas com `"sample_id"` (ou resolvendo recursivamente `"target_event"` com proteção contra ciclos via `visited` set) e gera um `WaveformEnvelope` composto (concatenação ou sobreposição por blocos na duração do clipe) cacheado em `WaveformCache`.
    - `programmer`: consulta `"programmer_key"` na `"audio_table"` do projeto para obter o `"sample_id"` associado e carregar a waveform real; caso a chave não possua áudio vinculado ainda, renderiza uma waveform vetorial sintética sutil indicando instrumento programático ativo.
    - `event`: percorre recursivamente todas as pistas e instrumentos do evento filho (`single`, `multi`, `scatterer`, `programmer` e sub-eventos, com detecção de ciclos) em vez de buscar apenas `single`.
- **Renderização Poligonal Suave (`WaveformEnvelope::render_clip`)**:
  - Construir dois `juce::Path` fechados anti-aliased por canal/envelope:
    1. **Peak Silhouette**: caminho contínuo unindo os picos máximos (topo) da esquerda para a direita e os picos mínimos (base) da direita para a esquerda, preenchido com `juce::ColourGradient` vertical suave (`peak_colour.withAlpha(0.45f)` nas cristas até `0.15f` próximo ao eixo central) e contorno de `1.25 px`.
    2. **RMS Core**: caminho interno fechado espelhado em torno do eixo zero usando os valores RMS, preenchido com `rms_colour.withAlpha(0.85f)` e linha central de referência sutil.
- **Ghost Geometry no Arraste de Clipe (`TimelinePanel` + `TimelineTrackLane`)**:
  - Separar visualmente **posição de origem**, **posição contínua do cursor (ghost)** e **posição alvo encaixada pelo snap**:
    - Durante `ClipComponent::mouseDrag` (tanto na mesma pista quanto entre pistas), `ClipComponent` não reposiciona definitivamente sua geometria definitiva sem feedback; em vez disso, o clipe na origem permanece visível como contorno tracejado de referência e um overlay/camada de drag em `TimelinePanel` (ou na `TimelineTrackLane` alvo) pinta continuamente o **Ghost Clip completo** (retângulo translúcido na cor da pista, miniatura da silhueta de waveform, linhas-guia verticais alinhadas às bordas até a régua e badge numérico com `tempo (ms)` e `delta`) exatamente por onde o mouse passa, além do marcador de destino com snap antes de confirmar no `mouseUp`.

### Decisão 5 — `DockingLayoutManager` v2, `WorkspaceBar`, 3 Templates e Presets Customizados

- **Modelo de Layout (`WorkspaceLayoutState`)**:
  - Cada painel identificável por ID estável (`transport`, `browser`, `timeline`, `inspector`, `bottom_deck`) possui:
    - `slot`: `Top`, `Left`, `Center`, `Right`, `Bottom` (ou `Floating`).
    - `visible`: `bool` (qualquer painel pode ser fechado/oculto individualmente, inclusive todos simultaneamente).
    - `tab_order` / `selected_tab` para slots com múltiplos painéis em abas (por exemplo, se o usuário mover `Inspector` para o slot `Left` junto ao `Browser`, o slot passa a operar com abas alternáveis).
  - `DockingDimensions`: `left_width`, `right_width`, `bottom_height` com `clamp` dinâmico que zera a reserva de espaço de slots sem nenhum painel visível (permitindo que o `TimelinePanel` ou `Mixer` expanda para 100% da área útil quando os demais estiverem fechados).
- **3 Templates Nativos (Somente Leitura) + Custom Presets**:
  1. `Default (Authoring)`: Transport no topo, Browser à esquerda, Timeline ao centro, Inspector à direita, Bottom Deck na base.
  2. `Stage Focus`: Browser e Bottom Deck ocultos (`visible = false`), Timeline expandida ocupando a área principal + Inspector à direita.
  3. `Mixing & Mastering`: Timeline oculta ou minimizada, Bottom Deck/Mixer promovido ao slot central/expandido com Browser à esquerda e Inspector à direita.
- **`WorkspaceBar` Permanente no Topo**:
  - Barra compacta (`28 px`–`32 px`) acima do conteúdo contendo:
    - Toggles individuais de visibilidade para cada painel (`Browser`, `Transport`, `Stage`, `Inspector`, `Deck`) e botão `Hide All / Restore`.
    - Seletor `ComboBox` de Layout listando os 3 templates nativos e os layouts salvos pelo usuário.
    - Botões `+ Save` (abre prompt leve de nome para salvar layout customizado), `Delete` (habilitado apenas para layouts do usuário, bloqueando exclusão dos 3 templates nativos) e `Reset` (restaura imediatamente os valores de fábrica do template selecionado ou do `Default (Authoring)`).
- **Cabeçalhos de Painel Dockável**: Cada painel em slot exibe controles compactos para alternar visibilidade (`[X]`), destacar (`Pop-out`) e escolher o slot de destino (`Move to Left / Center / Right / Bottom`).
- **Persistência (`oae_studio_layout.json`)**: Estende o esquema JSON mantendo retrocompatibilidade com arquivos que contenham apenas `left_width`, `right_width`, `bottom_height`, adicionando `active_preset`, `panels` e `custom_presets`.

### Decisão 6 — Estratégia de Validação Visual com `studio_ui_screenshot` (MCP Server)

- Conforme diretriz do usuário e contrato canônico em `openspec/specs/mcp-automation/spec.md`, além da execução dos smokes C++ (`run_smoke_timeline`, `run_smoke_ui_design_system` etc. via `scripts/verify.ps1 -Layer studio`), a verificação visual durante a aplicação desta change utilizará a ferramenta `studio_ui_screenshot` do servidor MCP do Studio (tanto em janela completa quanto por `component_id` como `timeline_view` e `inspector_panel`), capturando PNGs em disco ou base64 para inspecionar visualmente:
  1. O `InspectorPanel` em cards colapsáveis (`Clip` / `Track` / `Event`).
  2. Múltiplas tracks com cores distintas, track selecionada destacada de ponta a ponta e clipes usando a cor da track + badge de tipo.
  3. Waveforms poligonais em `Single`, `Multi`, `Scatterer`, `Programmer` e `Event`.
  4. Os 3 templates de layout e o estado com painéis ocultos via `WorkspaceBar`.

## Risks / Trade-offs

- **[Risco] Complexidade de composição de waveform em `Multi` / `Scatterer` e atualização em tempo real** → **Mitigação**: Resolver a sequência de fontes em cache leve indexado por assinatura da playlist/sample_id, evitando decodificação síncrona na thread de UI; usar `WaveformCache::get_or_load` assíncrono com fallback limpo enquanto os dados carregam.
- **[Risco] Perda de usabilidade se o usuário fechar todos os painéis e não encontrar como restaurar** → **Mitigação**: A `WorkspaceBar` superior permanece sempre ancorada no topo (ou oferece botão/atalho claro de restauração) com `Hide All / Restore`, toggles individuais e `Reset`.
- **[Risco] Regressão em testes de fumaça existentes que verificam dimensões fixas de splitters ou scroll do Inspector** → **Mitigação**: Preservar a API existente de `DockingDimensions::clamp` / `from_json` retrocompatível e atualizar/ampliar os smokes em `studio/Source/Main.cpp` para cobrir os novos invariantes.
- **[Risco] Alinhamento de `OaeSlider` em sliders rotatórios vs lineares** → **Mitigação**: Aplicar a nova lógica de extensão útil exclusivamente a sliders lineares horizontais/verticais; manter comportamento vetorial já validado para `RotaryHorizontalVerticalDrag`.
