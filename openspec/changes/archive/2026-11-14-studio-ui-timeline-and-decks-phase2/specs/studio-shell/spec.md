# Spec Delta: studio-shell

## ADDED Requirements

### Requirement: Barra de breadcrumb para navegação entre eventos pai e filho na Timeline
O `TimelinePanel` do OpenAudio Studio SHALL exibir o caminho hierárquico de navegação quando um evento aninhado (`Event Instrument`) for aberto a partir de um evento pai, permitindo retornar a qualquer ancestral clicando no respectivo segmento do breadcrumb ou no botão de voltar.

#### Scenario: Abrir evento filho por duplo-clique e retornar ao pai pelo breadcrumb
- **WHEN** o usuário abre um evento filho dando duplo-clique em um clipe `[EVT]` na timeline de um evento pai
- **THEN** a barra superior da timeline exibe a trilha `evento-pai > evento-filho` com segmentos clicáveis
- **AND** ao clicar no segmento do evento pai ou no botão de retorno, a seleção do `AuthoringModel` e a timeline voltam para o evento pai

### Requirement: Renderização vetorial de formas de curva e gradiente nas pistas e editores de automação
As pistas de automação da timeline (`AutomationLaneComponent`) e os editores de curva (`AutomationCurveEditor`, `AhdsrCurveEditor` e `EqPlot`) SHALL desenhar os segmentos conforme a forma configurada (`linear`, `hold`, `ease_in`, `ease_out`, `s_curve`) com preenchimento vertical em gradiente translúcido sob a curva e nós vetoriais destacados no hover/seleção.

#### Scenario: Renderização de segmento não linear e preenchimento sob a curva na timeline
- **WHEN** uma pista de automação contém pontos com forma `ease_in`, `ease_out`, `s_curve` ou `hold`
- **THEN** a `AutomationLaneComponent` desenha a trajetória correspondente entre os pontos usando primitivas vetoriais suaves e preenche a área sob a curva com gradiente translúcido derivado de `OaeTheme`

### Requirement: Edição direta por arraste de pontos na pista de automação da Timeline
A `AutomationLaneComponent` na timeline do OpenAudio Studio SHALL permitir selecionar e arrastar pontos de automação existentes para ajustar seu tempo (`x`) e valor (`y`), consolidando o movimento completo como uma única operação de Undo/Redo no `AuthoringModel`.

#### Scenario: Arrastar ponto de automação na timeline com Undo/Redo por gesto
- **WHEN** o usuário clica e arrasta um ponto existente em uma `AutomationLaneComponent` e solta o botão do mouse
- **THEN** a posição temporal e o valor do ponto são atualizados ao vivo durante o arraste e registrados no `AuthoringModel` como um único passo de Undo/Redo ao finalizar o gesto

### Requirement: Tokens semânticos e micro-layout FlexBox/Grid na Timeline, Headers e Decks Contextuais
Os componentes `TimelinePanel`, `TimelineRuler`, `TimelineTrackLane`, `TrackHeaderPanel`, `PlaylistDeck`, `LogicConditionsDeck`, `Spatializer3DDeck` e `EffectChainDeck` SHALL utilizar os tokens de cores, tipografia e espaçamento de `OaeTheme` e organizar suas barras e formulários internos via `juce::FlexBox` ou `juce::Grid`.

#### Scenario: Redimensionamento compacto de cabeçalhos de pista e decks contextuais
- **WHEN** a largura ou altura útil da timeline e do deck contextual inferior for reduzida pelo usuário
- **THEN** a toolbar da timeline, os itens de `TrackHeaderPanel` e os controles internos de `PlaylistDeck`, `LogicConditionsDeck`, `Spatializer3DDeck` e `EffectChainDeck` se redistribuem via `FlexBox`/`Grid` sem truncamento nem sobreposição
