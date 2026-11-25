# Proposal

## Why

Apesar dos avanços das fases anteriores do Design System (`studio-ui-design-system-and-docking` e `studio-ui-timeline-and-decks-phase2`), a experiência visual e de usabilidade do OpenAudio Studio ainda apresenta gargalos concretos em relação às referências de middleware e DAWs catalogadas em `docs/ui-references/` (FMOD Studio 2.03, Ableton Live 12, Logic Pro e Serum 2):

1. **Atraso perceptível no arraste de sliders lineares**: `OaeSlider::mouseDown` força `setSliderSnapsToMousePosition(false)`, fazendo o JUCE usar o `mouseDragSensitivity` fixo de `250 px` em vez da largura física do trilho, enquanto `OaeDarkLookAndFeel` não sobrescreve `getSliderThumbRadius` / `getSliderLayout` para alinhar o raio desenhado (`thumbDiameter = 12.0f`) à geometria lógica do controle.
2. **Property Inspector monolítico e pouco legível**: `InspectorPanel::create_event_editor` renderiza uma lista vertical contínua que despeja propriedades do evento, de todas as pistas e de todos os instrumentos diferenciados apenas por indentação de texto, sem agrupamento lógico claro nem distinção visual entre parâmetros de Evento, Track e Clip.
3. **Baixa legibilidade de seleção e identidade de pista no palco**: quando um evento possui 2 ou mais tracks, a pista selecionada não recebe destaque visual no palco (`TimelineTrackLane`); os clipes ignoram a cor da track no background (usando apenas cores fixas por tipo); e a seleção de clipe no palco muda apenas a espessura da borda de `1 px` para `2 px`.
4. **Waveforms ausentes em instrumentos compostos/dinâmicos e acabamento visual simples**: instrumentos `multi`, `scatterer`, `programmer` e eventos aninhados sem `single` direto não exibem waveforms no palco, e a renderização atual em linhas verticais isoladas carece do traçado poligonal contínuo em duas camadas (silhueta de pico com gradiente vertical + núcleo RMS) das referências.
5. **Falta de pré-visualização contínua de geometria durante o arraste de clipes**: durante o `mouseDrag` horizontal na mesma pista, o clipe salta conforme o snap sem mostrar referência à origem nem geometria fantasma acompanhando continuamente o cursor, e no arraste entre pistas não exibe a silhueta completa do clipe na trajetória.
6. **Layout rígido sem ocultação total nem presets**: `DockingLayoutManager` permite *pop-out* apenas do deck inferior, sem possibilidade de fechar/esconder painéis principais (ou todos), reconfigurar slots de forma livre, alternar entre templates de layout, restaurar o layout padrão ou criar e excluir layouts personalizados.

## What Changes

- **Pilar 1 — Precisão 1:1 no Arraste de Sliders (`OaeSlider` + `OaeDarkLookAndFeel`)**:
  - Mapeamento direto `1:1` entre o deslocamento horizontal/vertical do ponteiro do mouse e a extensão útil em pixels do trilho (`sliderRegionSize`) em velocidade padrão, preservando `fineDragScale = 0.1f` apenas quando `Shift` estiver pressionado e sem salto inicial no clique.
  - Sobrescrita de `getSliderThumbRadius` e `getSliderLayout` em `OaeDarkLookAndFeel` alinhados às constantes geométricas de `OaeTheme::Sliders`.
- **Pilar 2 — Property Inspector Modular em Cards Colapsáveis (`Clip` -> `Track` -> `Event`)**:
  - Refatoração do `InspectorPanel` para substituir a árvore contínua por três cartões colapsáveis independentes (*Accordion*) focados exclusivamente no contexto selecionado: Card `CLIP` no topo (barra lateral na cor da track, badge do tipo e subgrupos lógicos como *Source & Playback*, *Timing & Fades*, *Trigger Conditions* e *Clip Modulation*), Card `TRACK` no meio (nome, seletor de cor da track, mixagem e automações da pista) e Card `EVENT` na base (bus, macros, parâmetros locais, modulação do evento, automação e *User Properties*).
- **Pilar 3 — Paleta de Cores por Track, Destaque de Track/Clip Selecionados no Palco**:
  - Suporte a propriedade de cor por pista no `AuthoringModel` e paleta curada de tracks em `OaeTheme`, com atribuição cíclica automática ao criar pistas e seletor de cor no cabeçalho e no Inspector.
  - Destaque contínuo de ponta a ponta da pista selecionada (`TrackHeaderItem` + `TimelineTrackLane` com tint translúcido e bordas superior/inferior contínuas quando há 2 ou mais pistas).
  - Aplicação da cor da Track ao background dos clipes daquela pista, mantendo a diferenciação do tipo de instrumento (`Single`, `Multi`, `Scatterer`, `Programmer`, `Event`, `Command`) exclusivamente pelo badge/ícone na barra de título do clipe (estilo Ableton/Logic), além de seleção de clipe em alto contraste (barra de título iluminada/invertida, moldura dupla brilhante e alças laterais visíveis).
- **Pilar 4 — Waveforms Universais Poligonais e Ghost Preview no Arraste de Clips**:
  - Resolução universal de waveform no palco para `single`, `multi` (envelope composto da `playlist`), `scatterer` (`playlist`), `programmer` (lookup pela `audio_table`) e `event` (resolução recursiva em qualquer tipo de instrumento filho).
  - Novo renderizador vetorial de waveform em `WaveformCache` usando polígonos contínuos anti-aliased (`juce::Path`) em duas camadas (silhueta de pico com gradiente vertical translúcido + núcleo RMS interno).
  - Visualização contínua da geometria fantasma (*ghost clip* com silhueta, linhas-guia verticais até a régua e leitura temporal em `ms`) acompanhando a posição por onde o cursor passa tanto na mesma pista quanto entre pistas, preservando uma sombra de referência na origem até o `mouseUp`.
- **Pilar 5 — Gerenciador Avançado de Layouts, 3 Templates Nativos, Custom Layouts e Visibilidade Total**:
  - Adição de `WorkspaceBar` compacta no topo permitindo fechar/ocultar ou reexibir individualmente qualquer painel (`Browser`, `Transport`, `Timeline/Palco`, `Inspector`, `Bottom Deck`), ocultar todos os painéis com restauração em um clique, reconfigurar o slot de encaixe de cada painel (`Left`, `Center`, `Right`, `Bottom`) ou destacá-lo (*pop-out*).
  - Inclusão de **3 templates nativos** imutáveis (`Default (Authoring)`, `Stage Focus` e `Mixing & Mastering`), suporte a **criar/salvar novos layouts**, **excluir layouts do usuário** e acionar **Reset Layout** para restaurar o padrão, tudo persistido em `oae_studio_layout.json`.
- **Pilar 6 — Validação Visual via MCP Snapshot Tool**:
  - Uso da ferramenta `studio_ui_screenshot` do servidor MCP do Studio (especificada em `openspec/specs/mcp-automation/spec.md`) em complemento aos smokes automatizados para inspecionar e validar visualmente os estados renderizados da UI (cards do Inspector, seleção de track/clip, waveforms, ghost drag e templates de layout).

## Capabilities

### New Capabilities
<!-- Nenhuma nova capacidade necessária; todos os requisitos pertencem a studio-shell. -->

### Modified Capabilities
- `studio-shell`: Adiciona requisitos observáveis para rastreamento `1:1` de sliders lineares, Inspector modular em cards colapsáveis (`Clip` -> `Track` -> `Event`), cores de pista aplicadas aos clipes com badge por tipo e destaque visual de pista/clipe selecionados, waveforms poligonais universais no palco, *ghost geometry* contínua durante arraste de clipes e gerenciamento de layouts com reconfiguração de slots, ocultação total, 3 templates nativos, criação/exclusão de presets e reset.

## Impact

- **Exclusões**: Não altera DSP/runtime C++, ABI C, compilador/formato de banco (`.bank`) nem o pacote Unity. A propriedade de cor de pista permanece metadado de autoria do projeto JSON ignorado na compilação binária.
- **Código Afetado (Camada `studio`)**:
  - `studio/Source/UI/Theme.h`, `studio/Source/UI/OaeSlider.h`, `studio/Source/UI/OaeDarkLookAndFeel.h/.cpp`
  - `studio/Source/Model/AuthoringModel.h/.cpp`
  - `studio/Source/UI/InspectorPanel.h/.cpp`
  - `studio/Source/UI/TrackHeaderPanel.h/.cpp`, `studio/Source/UI/TimelineTrackLane.h/.cpp`, `studio/Source/UI/TimelinePanel.h/.cpp`, `studio/Source/Timeline/WaveformCache.h/.cpp`
  - `studio/Source/UI/DockingLayoutManager.h/.cpp`, `studio/Source/MainComponent.h/.cpp`, `studio/Source/Main.cpp`
- **Documentação e Validação**:
  - Atualização de `docs/UI_GUIDELINES.md` e `docs/ai/memory.md`.
  - Verificação por suíte `scripts/verify.ps1 -Layer studio`, `scripts/verify.ps1 -Layer docs` e inspeção visual via `studio_ui_screenshot` do servidor MCP do Studio.
