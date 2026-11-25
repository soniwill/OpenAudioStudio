# Design

## Context

Na Fase 1 (`studio-ui-design-system-and-docking`), o OpenAudio Studio recebeu `docs/UI_GUIDELINES.md`, `studio/Source/UI/Theme.h`, `OaeDarkLookAndFeel` vetorial e micro-layout `FlexBox`/`Grid` na barra de transporte, inspetor e mixer. Uma auditoria em `studio/Source/UI/` mostrou que o centro de edição temporal (`TimelinePanel`, `TimelineRuler`, `TimelineTrackLane`, `TrackHeaderPanel`) e os quatro decks contextuais especializados (`PlaylistDeck`, `LogicConditionsDeck`, `Spatializer3DDeck`, `EffectChainDeck`) ainda concentram cores hexadecimais literais, posicionamentos manuais fixos e duas limitações funcionais diretas:
1. O duplo-clique em um clipe `[EVT]` abre o evento filho, mas descarta o contexto do evento pai.
2. A `AutomationLaneComponent` desenha apenas linhas retas (`lineTo`), ignorando `shape`, e possui `mouseDrag`/`mouseUp` vazios.

## Goals / Non-Goals

**Goals:**
- Implementar trilha de navegação (*breadcrumb*) pai -> filho na toolbar de `TimelinePanel` com suporte a retorno imediato em `1 clique`.
- Unificar a renderização vetorial de curvas (`AutomationLaneComponent`, `AutomationCurveEditor`, `AhdsrCurveEditor`, `EqPlot`) com suporte a shapes (`linear`, `hold`, `ease_in`, `ease_out`, `s_curve`), gradiente translúcido sob a curva (*ribbon fill*), linhas de grade sutis e nós com halo vetorial.
- Habilitar arraste direto (`mouseDrag` / `mouseUp`) de pontos na `AutomationLaneComponent` com transação única de Undo/Redo por gesto.
- Estender `OaeTheme` com tokens semânticos de clipes/régua e eliminar hexadecimais hardcoded em `TimelinePanel`, `TimelineRuler`, `TimelineTrackLane`, `TrackHeaderPanel` e nos quatro decks contextuais.
- Migrar a toolbar da Timeline, `TrackHeaderItem::resized()` e os formulários/colunas de `PlaylistDeck`, `LogicConditionsDeck`, `Spatializer3DDeck` e `EffectChainDeck` para `juce::FlexBox` e `juce::Grid`.

**Non-Goals:**
- Não alterar o formato binário `.bank`, o schema JSON do projeto nem o motor de áudio C++ nativo (os atributos `shape` de automação e `target_event` já existem no modelo e no compilador).
- Não substituir o posicionamento temporal em pixels de clipes e marcadores por `FlexBox` (coordenadas de tempo `ms -> px` na área de pistas e régua são domínio de timeline contínua, não de fluxo de controles).

## Decisions

### 1. Histórico de Navegação do Breadcrumb em `TimelinePanel` / `AuthoringModel`
- **Decisão**: Manter uma trilha ordenada de caminhos de eventos (`std::vector<std::string> breadcrumb_trail_`) coordenada durante a seleção de eventos:
  - Quando a seleção muda a partir de uma abertura de `Event Instrument` (ou quando o evento selecionado é referenciado por um `Event Instrument` / playlist do evento atual no topo da trilha), o novo `event_path` é empilhado na trilha.
  - Quando o usuário clica em um ancestral na barra de breadcrumb (ou no botão `Back`), a trilha é truncada até aquele ancestral e `model_.select({ ItemType::Event, ancestor_path, -1, -1 })` é invocado.
  - Quando o usuário seleciona diretamente um evento sem relação de parentesco na árvore `AuthoringTreeBrowser`, a trilha é reiniciada com `[selected_event_path]`.
- **Justificativa**: Evita ambiguidade quando um mesmo sub-evento reutilizável (ex.: `event:/sfx/footstep_impact`) é referenciado por múltiplos eventos pais, pois preserva exatamente o caminho pelo qual o autor entrou.

### 2. Helper Compartilhado de Geometria de Curvas e Ribbon Fill
- **Decisão**: Centralizar a construção de `juce::Path` para segmentos interpolados (`linear`, `hold`, `ease_in`, `ease_out`, `s_curve`) usando `cubicTo` / `lineTo` e o preenchimento fechado sob a curva (`ribbonPath`) com `juce::ColourGradient` vertical (opacidade `OaeTheme::States::selectedFillOpacity` no topo até `0.0f` na base).
- **Justificativa**: Garante consistência visual idêntica entre `AutomationLaneComponent` na Timeline e `AutomationCurveEditor` no Inspector sem duplicar matemática de interpolação.

### 3. Transação de Undo/Redo por Gesto na `AutomationLaneComponent`
- **Decisão**: Em `mouseDown` sobre um nó existente, capturar o índice do ponto e o snapshot inicial; durante `mouseDrag`, atualizar a visualização e o valor em tempo real mantendo os pontos adjacentes como limites horizontais; em `mouseUp`, consolidar a modificação no `AuthoringModel` em um único passo de Undo/Redo (mesmo padrão já validado em `AutomationCurveEditor` e `TimelineRuler`).

### 4. Separação entre Layout Temporal (`ms -> px`) e Micro-Layout (`FlexBox`/`Grid`)
- **Decisão**: Usar `juce::FlexBox` e `juce::Grid` na toolbar do `TimelinePanel`, nos controles internos de `TrackHeaderItem` e nos painéis de controle de `PlaylistDeck`, `LogicConditionsDeck`, `Spatializer3DDeck` e `EffectChainDeck`, preservando o cálculo proporcional `time_to_x` / `x_to_time` para a régua e para os clipes nas pistas.

## Risks / Trade-offs

- **[Risco: Colisão de ordenação ao arrastar pontos horizontalmente na `AutomationLaneComponent`]**  
  *Mitigação*: Limitar (`std::clamp`) a coordenada `x` do ponto arrastado ao intervalo `[prev_point.x, next_point.x]` (com margem mínima), garantindo que a lista de pontos permaneça estritamente ordenada.
- **[Risco: Espaço horizontal reduzido na toolbar do `TimelinePanel` quando o breadcrumb tiver múltiplos níveis]**  
  *Mitigação*: Exibir apenas o nome curto (segmento final após `/`) em cada chip do breadcrumb, com `tooltip` exibindo o caminho completo e colapso elíptico (`...`) para níveis intermediários quando a largura útil for estreita.
