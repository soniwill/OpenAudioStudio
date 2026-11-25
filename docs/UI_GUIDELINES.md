# Diretrizes de interface do OpenAudio Studio

Estas regras orientam a implementação e a revisão de painéis, diálogos e controles do Studio em JUCE. Use os tokens de [`OaeTheme`](../studio/Source/UI/Theme.h) ao criar ou migrar um componente.

## Paleta

Escolha a cor pelo papel na interface. Os valores abaixo usam `#RRGGBB`. No cabeçalho, o primeiro byte de `0xAARRGGBB` representa a opacidade. Converta o token com `juce::Colour` na configuração do componente ou ao desenhá-lo. Registre novos papéis no tema antes de usá-los em painéis ou diálogos.

| Token em `OaeTheme::Colors` | Hexadecimal | Uso |
| --- | --- | --- |
| `bgDark` | `#161719` | Fundo da janela |
| `bgSurface` | `#202225` | Superfície de painel |
| `bgSurfaceRaised` | `#282B30` | Controles e superfícies elevadas |
| `borderSubtle` | `#2F3237` | Contornos e divisores |
| `borderActive` | `#40444B` | Contorno em destaque |
| `accentCyan` | `#00F5D4` | Seleção, controle ativo e playhead |
| `accentAmber` | `#FFB703` | Solo, loops e avisos |
| `accentCoral` | `#FF5964` | Mute, transições e saturação |
| `accentGreen` | `#38B000` | Reprodução e nível de sinal seguro |
| `accentPurple` | `#B5179E` | Snapshots e automação |
| `tintCyan` | `0x2900F5D4` | Pílulas ativas, seleção e badge ciano translúcido |
| `tintGreen` | `0x3838B000` | Badge/botão ativo de reprodução translúcido |
| `tintAmber` | `0x2EFFB703` | Badge/botão de pausa ou aviso translúcido |
| `tintPurple` | `0x3DB5179E` | Badge/efeito roxo translúcido |
| `textPrimary` | `#F0F2F5` | Texto principal e valores |
| `textSecondary` | `#A0A4AB` | Rótulos secundários e unidades |
| `textMuted` | `#6B7280` | Texto de controles desabilitados |
| `indicator` | `#FFFFFF` | Ponteiro, cursor e marca de pico |
| `clipSingle` | `#1D4E89` | Clipe de instrumento Single |
| `clipMulti` | `#5C2751` | Clipe de instrumento Multi |
| `clipScatterer` | `#2A9D8F` | Clipe de instrumento Scatterer |
| `clipProgrammer` | `#3B6E8C` | Clipe de instrumento Programmer |
| `clipEvent` | `#D06224` | Clipe de Event Instrument |
| `clipCommand` | `#A27B5C` | Clipe de Command Instrument |
| `clipFallback` | `#3F4248` | Clipe sem categoria específica |
| `waveformPrimary` | `#80FFDB` | Realce da forma de onda padrão |
| `waveformEvent` | `#FFD166` | Forma de onda de evento aninhado |
| `waveformEventPeak` | `#FFF3B0` | Pico da forma de onda de evento aninhado |
| `trackFaderAccent` | `#3A86FF` | Trilho do fader de volume da pista |
| `trackPanAccent` | `#8338EC` | Trilho do controle de pan da pista |

Identifique estados como mute, solo e saturação também com rótulo, ícone ou indicador de estado. Preserve a legibilidade do texto ao compor estados de seleção e foco. Em curvas de automação, envelope AHDSR e resposta de frequência (`EqPlot`), use preenchimento vertical em gradiente translúcido com `OaeTheme::States::curveRibbonTopOpacity` (`0.24f`) no topo até `curveRibbonBottomOpacity` (`0.02f`) na base, grade sutil em `curveGridOpacity` (`0.18f`) e halo de foco/hover em `nodeHoverHaloOpacity` (`0.30f`).

## Espaçamento e micro-layout

Use a grade de 4 px lógicos para margens, preenchimento e distâncias entre controles. `OaeTheme::Spacing` fornece `unit` (4), `small` (8), `medium` (12), `large` (16) e `extraLarge` (24). A espessura de um traço vetorial segue a geometria do desenho.

Organize os componentes filhos de cada painel em `resized()` com `juce::FlexBox` para fluxos em linha ou coluna e `juce::Grid` para alinhamento em duas dimensões. Defina mínimos, crescimento e quebra de linha conforme o conteúdo. Exercite a largura mínima do painel e o arraste dos splitters para conferir que os controles continuam operáveis, legíveis e sem sobreposição. O `DockingLayoutManager` coordena a distribuição dos painéis na janela.

## Tipografia e Primitivas Reutilizáveis

Use a fonte sem serifa padrão da interface JUCE. Defina alturas lógicas com `OaeTheme::Typography` e `juce::FontOptions`: `micro` (9 px) para badges e legendas de máscara, `caption` (10 px) para rótulos de seção, `compact` (11 px) para `ValueBox` e controles compactos, `control` (12 px) para botões e árvore, `paramTitle` (13 px) para títulos de parâmetros no deck, `body` (14 px) e `heading` (16 px). Reserve negrito para hierarquia e dígitos de largura constante quando a variação puder deslocar a leitura.

Raios e métricas adicionais em `OaeTheme::Edges` e `OaeTheme::Sliders`: `cornerRadius` (3 px), `cardCornerRadius` (4 px), `switcherCornerRadius` (5 px), `pillCornerRadius` (10 px), `selectedBorderStroke` (1.5 px), `largeTrackThickness` (6 px) e `largeThumbDiameter` (14 px).

Primitivas em `studio/Source/UI/OaePrimitives.h` (baseadas em classes JUCE padrão para compatibilidade com `McpServer`):
- `OaeToolbarButton` (`juce::TextButton`): variantes `Neutral`, `AccentCyan`, `TintCyan` e `AccentPurple`.
- `OaeStatusBadge` (`juce::TextButton`): pílula `20 px` (`Cyan`, `Green`, `Amber`, `Purple`, `Neutral`) com ponto `●` opcional e modo interativo.
- `OaeSegmentedControl` (`juce::Component` com `juce::TextButton` filhos): seletor segmentado (`Timeline | Layers`, opções rotuladas).
- `OaeValueBoxSlider` (`OaeSlider`): caixa `bgSurface` + `borderSubtle` (`22 px`), mantendo arraste `1:1`, `Shift` `0.1x` e reset por duplo-clique.

## Desenho vetorial e interação de áudio

- Desenhe controles, ícones e medidores com primitivas de `juce::Graphics` e `juce::Path`, a partir dos limites locais, para acompanhar o redimensionamento e a escala da tela. Use trilho sutil, arco proporcional ao valor e ponteiro central visível nos knobs. Preserve uma indicação de posição clara nos faders e scrollbars.
- Exiba a unidade aplicável junto ao valor, como dB, dBFS ou Hz. Durante o arraste de sliders e knobs, use `Shift` para ajuste fino em décimos de unidade. Em controles com `default_value`, restaure-o por duplo clique e registre a alteração no histórico de Undo/Redo.
- Atualize a visualização do nível de 30 a 60 FPS na thread de interface. Use o perfil existente de `BallisticMeter` exposto por `OaeTheme::Animation`, com referência visual de −80 dBFS no piso, ataque instantâneo, decaimento de 20 dB/s, integração RMS de 300 ms e retenção de pico de 1,2 s. Desenhe a indicação de pico de forma distinta da barra de nível e use o tempo decorrido para calcular o decaimento.

## Exemplo de aplicação

Inclua `UI/Theme.h` em um componente do alvo `oae_studio` e aplique os tokens a um `juce::Label` já existente. O exemplo pressupõe `levelLabel` como campo do componente e executa na thread de interface.

```cpp
using oae::studio::OaeTheme;
levelLabel.setColour(juce::Label::textColourId,
                     juce::Colour(OaeTheme::Colors::textSecondary));
levelLabel.setFont(juce::FontOptions(OaeTheme::Typography::control));
```

Para conferir a integração, configure o Studio com `cmake -S . -B build/Studio_Debug -DBUILD_STUDIO=ON -DCMAKE_BUILD_TYPE=Debug` e execute `cmake --build build/Studio_Debug --config Debug --target oae_studio` na raiz. Após aplicar o exemplo, abra o painel e confira o rótulo cinza secundário a 12 px lógicos.

## Verificação de controles de áudio

1. Abra um projeto com um parâmetro contínuo local ou global que tenha `default_value`. Selecione o evento para exibir os sliders de prévia no Transport.
2. Arraste o slider e mantenha `Shift` pressionado durante o movimento. O arraste passa a aplicar um décimo do deslocamento, sem salto ao pressionar ou soltar a tecla. Parâmetros discretos e rotulados continuam ajustáveis pelo passo inteiro.
3. Dê duplo clique no slider para restaurar o padrão. Com o foco em um slider de prévia, use `Ctrl+Z` e `Ctrl+Y` no Windows, ou `Cmd+Z` e `Cmd+Shift+Z` no macOS. O valor muda na audição e no Live-Link, quando conectado. O histórico de prévia acompanha os sliders exibidos e é reiniciado quando o Transport os reconstrói. Edições de autoria, como o reset de ganho de uma pista, usam o Undo/Redo do projeto.
4. Abra o Mixer e reproduza um evento. Observe a barra de pico subir com o sinal e cair gradualmente quando ele baixa. O traço de retenção permanece visível por cerca de 1,2 s antes de acompanhar a queda.

Execute `pwsh -File scripts/verify.ps1 -Layer studio` para compilar e rodar os smokes. O smoke de UI verifica o arraste fino em slider linear e knob, o reset, o Undo/Redo no Transport e a balística do medidor.


## TrackColors Palette & Timeline Clip Standard

Tracks use a cyclic 8-color palette defined in `OaeTheme::TrackColors` (`paletteSize = 8`), stored per track inside event `user_props` (`"track_<i>_color"` / `"track_<i>_name"`) so event tracks remain compatible with the strict `oae_bankc` `project-v1` schema:

| Index | Label | Hex Token | ARGB Literal |
| :--- | :--- | :--- | :--- |
| `0` | Cyan | `#00B4D8` | `0xFF00B4D8u` |
| `1` | Orange | `#F77F00` | `0xFFF77F00u` |
| `2` | Purple | `#7209B7` | `0xFF7209B7u` |
| `3` | Teal | `#2EC4B6` | `0xFF2EC4B6u` |
| `4` | Red | `#E71D36` | `0xFFE71D36u` |
| `5` | Indigo | `#4361EE` | `0xFF4361EEu` |
| `6` | Amber | `#FFBE0B` | `0xFFFFBE0Bu` |
| `7` | Magenta | `#F72585` | `0xFFF72585u` |

- **Deterministic Assignment & Parsing**: `OaeTheme::TrackColors::colorForIndex(index)` and `hexForIndex(index)` assign cyclic defaults; `parseHexColor(hex, fallbackIndex)` parses `#RRGGBB` and falls back cleanly on invalid input.
- **Clip Title Bar Pattern**: Every timeline clip paints its header strip in the parent track's accent color, prefixes a standardized type badge (`[SGL]`, `[MLT]`, `[SCT]`, `[PRG]`, `[EVT]`, `[CMD]`) before the clip name, and picks high-contrast foreground text (`OaeTheme::Colors::clipTitleTextDark = 0xff0b0c10` on bright track headers vs light text on dark headers).
- **Stage Selection, Waveforms & Ghost Drag**:
  - The active track lane uses a subtle fill tint (`OaeTheme::States::selectedTrackTintOpacity = 0.14f`) alongside a crisp high-contrast outline (`OaeTheme::States::selectedClipBorderThickness = 2.5f`) on the selected clip.
  - All clip types with resolvable audio (including multi, scatterer, programmer, and nested event references, with cycle guards) render closed polygonal waveforms below the clip title bar.
  - Dragging/moving/resizing renders a translucent *ghost preview* at `OaeTheme::States::ghostClipOpacity = 0.45f` plus timeline guide lines.
- **Linear `OaeSlider` Tracking**: Horizontal/vertical `OaeSlider` drags track pointer position `1:1` across `sliderBounds`; holding `Shift` scales movement by `OaeTheme::Sliders::fineDragScale = 0.1f`. For non-ASCII UI punctuation on MSVC `/utf-8` (such as `—` in headers), wrap strings with `juce::String::fromUTF8(...)`.

## Modular Collapsible Inspector Cards & Architecture

`InspectorPanel` organizes properties of the selected project item into modular, collapsible cards sharing preserved collapse state, scroll position, and atomic Undo/Redo integration.

### 1. Fixed Top Toolbar & Lock Inspector
- **Toolbar Layout**: When floating or standalone, a `24 px` `dockHeaderBg` bar houses the title and **Lock Inspector** button (`VectorIconButton` with `VectorIconType::LockClosed` / `VectorIconType::LockOpen`). When solo-docked, `DockHeader` (`24 px`) hosts `PROPERTY INSPECTOR` and the lock button without duplicating headers.
- **Freeze Filtering**: When locked (`is_locked() == true`), `on_selection_changed` ignores selection events from the Timeline, Project Browser, and Mixer, locking the Inspector onto the active target.
- **Interactive Editing**: All controls on the locked cards (sliders, text inputs, swatches, AHDSR envelopes) remain fully interactive and editable.
- **Instant Resync**: Toggling lock off immediately resynchronizes the Inspector with the current project selection.

### 2. Standardized Card Headers (`InspectorCardHeader`)
Every card features an `InspectorCardHeader` component providing:
- **Collapsible Chevron**: Crisp vector chevron (`VectorIconType::ChevronDown` / `VectorIconType::ChevronRight`) toggling card visibility while maintaining scroll stability.
- **Context Title & Semantic Badge**: Uppercase context title and right-aligned pill badge (e.g., `EVENT`, `TRACK`, `CLIP — SINGLE`).
- **Context Action Menu (`...`)**: A 3-dots action button (`VectorIconType::MoreHorizontal`) presenting:
  1. **Reset to Default**: Reverts all card parameters to default values in a single atomic `AuthoringModel` transaction.
  2. **Copy Values**: Serializes the card's state into the in-memory `InspectorClipboard`.
  3. **Paste Values**: Deserializes and applies clipboard data (enabled only when clipboard payload matches the card's `card_type`) in a single atomic transaction.

### 3. In-Memory Clipboard (`InspectorClipboard`)
- Isolated process-level clipboard holding `InspectorClipboardPayload` (`card_type` string identifier and `nlohmann::json` values).
- Operates independently of the operating system clipboard without overwriting user text or files.

### 4. Decoupled Card Modules (`studio/Source/UI/Inspector/`)
Each inspector surface is extracted into a dedicated card adhering to `IInspectorCard`:
- **`EventHeaderCard`**: 2D/3D mode, master bus route, user notes, custom user properties, instance limits, cooldown, and voice steal mode.
- **`TrackCard`**: Track name, volume, pan, mute/solo, `TrackColorSwatchStrip` (8 curated colors), and parameter automation curves.
- **`ClipCard`**: Audio instruments (`Single`, `Multi`, `Scatterer`, `Programmer`, `Command`, `Silence`), fades, AHDSR envelope, async toggle, and parameter trigger sheet.
- **`BusCard`**: Bus volume, parent bus routing, and DSP effects rack.
- **`ParameterCard`**: Global parameter attributes, range boundaries, and default value.
- **`SnapshotCard`**: Snapshot mode (Override vs Additive) and fade-in duration.
- **`VcaCard`**: VCA volume fader and bus assignments.
- **`SampleCard`**: Audio asset streaming toggle.
- **Card Chrome Tokens**: `OaeTheme::Colors::cardHeaderBg` (`0xff232733`), `cardBodyBg` (`0xff1a1d26`), and `cardBorder` (`0xff2c3140`), with `28 px` `InspectorCardHeader` (plain `accentCyan` text badge, no side stripe) and `OaeValueBoxSlider` rows (`10 px` padding/gap), plus contextual `MaskTransitionCard` (`MÁSCARA & TRANSIÇÃO`) when in `Layers` mode or when a track mask is active.

## WorkspaceBar & Docking Layout System

`DockingLayoutManager` arranges the workspace as groups of tabs and nested, resizable splits. The seven stable panel keys are `transport`, `browser`, `timeline`, `inspector`, `bottom_deck`, `diagnostics`, and `project_toolbar`; the Diagnostics & Log component keeps the ID `deck_diagnostics`. Each panel appears once in the workspace tree. Hidden or empty branches release their area, and saved proportions adapt to the current window size subject to panel minimums.

- **Permanent Toolbar Docks (`ProjectToolbar` and `TransportBar`)**:
  - `ProjectToolbar` (`project_toolbar`, housing `OPENAUDIO` brand, `New`, `Open`, `Save`, `Build Bank [F7]`, and `Help ?`) and `TransportBar` (`transport_bar`, housing `24×24` outline/tint `Play`, `Pause`, `Stop`, `Loop`, `Key Off`, `bgDark` `TransportReadout`, `statusLabel`, `Params (N)` badge, and horizontal stereo `KSystemMeter`) start side by side in the `Top` slot below `WorkspaceBar`.
  - Toolbar docks suppress `DockHeader` (`SlotHeaderBar`), use a compact unified bar height (`36 px` on `bgSurface`), and expose a `12 px` left-edge `GripHandle` with a dotted `::` texture and `DraggingHandCursor`.
  - **DockHeader (`24 px`) & BottomDeckHeader (`34 px`)**: Single-panel docks (`BROWSER`, `STAGE`, `PROPERTY INSPECTOR`) use a single `24 px` `DockHeader` (`dockHeaderBg`); solo `ContextualBottomDeck` suppresses external `SlotHeaderBar` and renders a unified `34 px` `BottomDeckHeader` with draggable `BOTTOM DECK` handle, 7 ordered tabs (`Mixer`, `Effect Chain`, `Logic Conditions`, `Playlist`, `3D Spatializer`, `Parameters (N)`, `Diagnostics`), right-click `Tabs` menu, and a single `Pop-Out ^` button.
  - **WorkspaceBar (`28 px`)**: Houses `WORKSPACE`, 5 visibility pills (`Trn`, `Brw`, `Stg`, `Ins`, `Dck`), `Preset:`, `+ Save Preset`, and overflow menu `...` (`btn_workspace_menu`) with `Log`, `Prj`, `Hide All / Restore`, `Dock Slots`, `Delete Custom Preset`, and `Reset Layout`.
  - **Grip gestures**: drag to reposition or float, double-click to toggle minimized/maximized mode (in minimized mode, `TransportBar` keeps `Play`, `Pause`, `Stop`, `Loop`, and `Timecode`), and right-click for the context menu (`Minimize/Expand Bar`, `Float Window/Dock Back to Top`).
  - **Permanent persistence (`can_close = false`)**: toolbar docks have no `'X'` close button, remain visible during `Hide All`, and their `WorkspaceBar` buttons (`[Trn]`, `[Prj]`) toggle minimized/focused state (or dock back if floating) instead of closing. Floating toolbar windows omit the native OS `'X'` close button and provide a dedicated `Dock Back` button (`btn_floating_dock_back`).
- **Vector Icon Buttons & Global Tooltips**:
  - `VectorIcons` (`studio/Source/UI/VectorIcons.h`) renders crisp `juce::Path` glyphs at 30x26 px without external image assets, using semantic active backgrounds (`accentGreen` for Play, `accentAmber` for Pause, `accentCyan` for Loop).
  - `MainComponent` owns a global `juce::TooltipWindow` styled by `OaeDarkLookAndFeel::drawTooltip` / `getTooltipBounds`, displaying control names and keyboard shortcuts (`Play (Space)`, `Save Project (Ctrl+S)`, `Build Bank (F7)`).

- **Drag targets and preview**: Drag a dock panel by its title. Drop over a tab bar to insert it into that tab group; drop on a panel edge to split that panel; drop on an outer workspace edge to split the full dock area. The preview shows the target and proposed region before release. Esc or release without a valid target cancels the operation and preserves the prior layout. The Diagnostics & Log tab in Bottom Deck can be moved as a workspace panel; other Bottom Deck tabs remain contextual tools.
- **Workspace controls**: Keep the existing panel visibility, `Dock`, `Hide All`, and `Restore` controls. `Hide All` hides the docked panels and `Restore` brings back the previous workspace arrangement.
- **Built-in templates**: Preserve the existing read-only templates `Default (Authoring)`, `Stage Focus`, and `Mixing & Mastering`. A custom arrangement is created from the current workspace with `+ Save` and a name. There is no additional built-in default for the reference arrangement.
- **Persistence**: Save the split tree, proportions, tab order and selection, and visibility in workspace state and custom presets. Continue to read existing saved layouts and apply the default layout as a fallback when the active saved state is invalid.
- **Graphite dock chrome**: Use semantic `OaeTheme` tokens for dock headers and Bottom Deck tabs. A single-panel title belongs in the header strip. In a tab group, distinguish active and inactive tabs by background and text contrast; make keyboard focus visible independently of selection and retain accessible labels and actions.

## Formatos de Áudio e Drag-and-Drop de Assets

A interface do Studio emprega `ma_decoder` (`miniaudio`) para decodificação nativa de arquivos de áudio nos formatos `.wav`, `.ogg`, `.mp3` e `.flac`:

- **WaveformCache e PlaylistDeck**: Amostras em formatos comprimidos (.ogg, .mp3, .flac) geram envelopes de pico e RMS poligonais idênticos aos do runtime nativo em `WaveformCache` e exibem durações formatadas em milissegundos no `PlaylistDeck` em vez de marcar como "Unreadable".
- **Drag-and-Drop na Timeline (`TimelineTrackLane`)**: Implementa `juce::FileDragAndDropTarget`. Ao arrastar arquivos de áudio válidos sobre uma pista, exibe prévia translúcida do drop e, ao soltar, copia para `assets/` (com resolução não destrutiva de colisões), cadastra o sample no `project.json` e cria um `Single Instrument` na posição temporal do mouse com a duração original do arquivo em um único passo atômico de Undo/Redo. Arquivos não suportados são rejeitados em `isInterestedInFileDrag`.
- **Drag-and-Drop no Navegador de Assets (`AuthoringTreeBrowser`)**: Implementa `juce::FileDragAndDropTarget` sobre a árvore de assets, permitindo importação em lote para `assets/` e cadastro no catálogo sem criar clipes na timeline.
- **Menu de Contexto de Assets**: Disponibiliza a ação explícita **Import Audio...** tanto no menu de contexto da categoria *Audio Assets* quanto no menu suspenso `+` do topo do painel, abrindo seletor nativo `juce::FileChooser` com filtro `*.wav;*.ogg;*.mp3;*.flac`.
