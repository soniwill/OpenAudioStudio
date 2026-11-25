# Proposal: studio-global-parameters-deck-and-contextual-strip

## Why

Atualmente, o Studio agrupa tanto os parâmetros locais do evento ativo quanto todos os parâmetros globais do projeto diretamente dentro do dock da barra de transporte (`TransportBar`). Esse desenho apresenta problemas severos de escalabilidade e usabilidade:
1. Em uma barra superior compacta de 34 px, os parâmetros são renderizados em sliders microscópicos de 80 px de largura por 15 px de altura, sem exibição de valores numéricos (`NoTextBox`) e sem suporte a rótulos de texto para parâmetros do tipo `labeled`.
2. Em projetos com mais de 5 a 10 parâmetros globais (cenário padrão em jogos para variáveis de mundo como clima, hora do dia, saúde, intensidade de combate), os sliders transbordam para uma rolagem horizontal imprensada entre o timecode e o status de áudio, inviabilizando testes e audição multivariável.
3. Não há separação conceitual entre o transporte de reprodução (play/pause/loop/timecode), a edição contextual do evento ativo e a simulação de variáveis globais do projeto.

Esta change separa formalmente essas três responsabilidades em uma arquitetura híbrida de 3 níveis: purifica a barra de transporte, introduz uma fita retrátil de parâmetros na Timeline com escopo do evento e estabelece uma aba dedicada de simulação no Bottom Deck (`Global Parameters`) com suporte a cards ricos, busca, tipos especializados e desacoplamento flutuante (*pop-out* com `Dock Back`).

## What Changes

- **Limpeza e Redirecionamento na Barra de Transporte (`TransportBar`)**:
  - Remove o viewport horizontal de sliders embutido da barra de 34 px.
  - A barra passa a focar exclusivamente em controles essenciais de transporte (Play, Pause, Stop, Loop, KeyOff), timecode, status da engine e um botão de ação rápida `[Params (N)]` com badge indicativo de parâmetros globais que foca/abre a aba de parâmetros.
- **Fita Retrátil de Parâmetros Contextuais na Timeline (`TimelinePanel`)**:
  - Adiciona uma fita superior retrátil/colapsável logo acima da régua de tempo.
  - Exibe exclusivamente os parâmetros consumidos pelo evento ativo (parâmetros locais de `ev["parameters"]` e globais vinculados a curvas de automação, condições de lógica ou Parameter Sheets).
  - Permite expandir/recolher via botão `[v] Parameters (N)` e oculta-se automaticamente com altura 0 px quando o evento não consome parâmetros.
  - Cada slider da fita possui largura generosa (120-140 px), leitura numérica legível e reset por duplo-clique.
- **Deck de Parâmetros Globais no Bottom Deck (`ContextualBottomDeck`)**:
  - Adiciona uma nova aba nativa `Parameters` ao deck inferior, ao lado de Mixer, Playlist, Effects, Logic, Spatializer e Diagnostics.
  - Organização em **Grid Responsivo de Cards** (2 a 3 colunas) com barra de ferramentas superior contendo caixa de busca textual (`Filter parameters...`), filtro de visualização (`Todos`, `Usados no Evento`, `Modificados`) e ação `Reset All to Default`.
  - Controles especializados por tipo:
    - `continuous`: Fader horizontal largo com display numérico de precisão e limites visíveis.
    - `discrete`: Slider com paradas em inteiros e indicador de passo.
    - `labeled`: Botoeira segmentada (*segmented button*) exibindo o texto de cada rótulo nominal.
  - Exibição de estado dinâmico dos modificadores em tempo real (`velocity` animado, `seek_speed` e ícone de `hold_during_playback` retido).
- **Desacoplamento de Janela (*Pop-out*)**:
  - Suporte a destacar o deck de Parâmetros Globais em janela flutuante independente (`DockSlotId::Floating` via `DockingLayoutManager`), com botão `[Dock Back]` para retorno seguro ao deck inferior.

## Capabilities

### New Capabilities
<!-- Nenhuma nova capacidade conceitual criada; o escopo UI/Workspace é absorvido por studio-shell -->

### Modified Capabilities
- `studio-shell`: Atualiza os requisitos da barra de transporte para refletir a remoção do viewport embutido e a inclusão do botão de foco `[Params]`, adiciona requisitos formais para a Fita Retrátil de Parâmetros Contextuais na Timeline e define a aba `Parameters` no Bottom Deck com layout em grid de cards e suporte a pop-out flutuante.

## Impact

- **Código C++ do Studio (`studio/Source/UI/`)**:
  - `TransportBar.h` / `TransportBar.cpp`: remoção do `parameterViewport_`, introdução do botão `paramsButton_` com badge e callback de navegação.
  - `TimelinePanel.h` / `TimelinePanel.cpp`: novo componente interno de fita de parâmetros retrátil, cálculo de altura dinâmica e renderização com sliders de precisão.
  - `ContextualBottomDeck.h` / `ContextualBottomDeck.cpp`: novo `DeckTab::Parameters`, integração do novo painel `GlobalParametersDeck`.
  - Novo componente `GlobalParametersDeck.h` / `GlobalParametersDeck.cpp`: grid de cards, busca/filtro e suporte a controles contínuos, discretos e rotulados.
  - `DockingLayoutManager.h` / `DockingLayoutManager.cpp`: suporte a pop-out/floating para o deck de parâmetros.
- **Smokes e Testes**: Smokes do Studio Release e validação de layout MCP sem regressões de inicialização ou persistência de layout JSON.
