# Proposal

## Why

Após a consolidação da Fase 1 do Design System (`Theme.h`, `OaeDarkLookAndFeel`, `FlexBox`/`Grid` em `TransportBar`, `InspectorPanel` e `MixerDeckPanel`, além de docking com *pop-out*), os componentes centrais de edição na Timeline (`TimelinePanel`, `TimelineRuler`, `TimelineTrackLane`, `TrackHeaderPanel`) e os decks contextuais especializados (`PlaylistDeck`, `LogicConditionsDeck`, `Spatializer3DDeck`, `EffectChainDeck`) ainda operam com dezenas de cores hexadecimais literais, fatiamentos manuais rígidos (`setBounds` / `removeFromLeft`) e lacunas de usabilidade em fluxos essenciais de autoria: falta navegação por *breadcrumb* ao entrar em Nested Events, e a pista de automação na Timeline desenha apenas segmentos retos sem preenchimento translúcido e sem suporte a arraste direto de pontos.

## What Changes

- **Pilar 1 — Breadcrumb de Navegação de Nested Events na Timeline**:
  - Rastreamento da trilha de navegação pai -> filho quando o usuário abre um `Event Instrument` (`[EVT]`) por duplo-clique ou navega entre eventos aninhados.
  - Exibição de barra de *breadcrumb* interativa na toolbar superior de `TimelinePanel` com segmentos clicáveis, separadores vetoriais em chevron e botão de retorno imediato ao evento pai.
- **Pilar 2 — Curvas de Automação Vetoriais (Bézier, Gradiente Ribbon e Arraste Direto)**:
  - Renderização de segmentos curvos (`ease_in`, `ease_out`, `s_curve`) e degrau (`hold`) via `juce::Path` na `AutomationLaneComponent` (`TimelineTrackLane`) e em `AutomationCurveEditor` / `AhdsrCurveEditor`.
  - Preenchimento vertical em gradiente translúcido (*ribbon fill*) sob as curvas de automação, envelope AHDSR e resposta de frequência (`EqPlot`), com grade de referência sutil e nós vetoriais com halo de foco/hover.
  - Implementação de `mouseDrag` e `mouseUp` na `AutomationLaneComponent` para mover pontos diretamente na pista da Timeline com leitura numérica e transação única de Undo/Redo por gesto.
- **Pilar 3 — Unificação de `OaeTheme` e `FlexBox` na Timeline e Cabeçalhos de Pista**:
  - Extensão de `studio/Source/UI/Theme.h` (e documentação em `docs/UI_GUIDELINES.md`) com tokens semânticos para categorias de clipes na Timeline e elementos de régua/grade.
  - Substituição das cores hexadecimais literais em `TimelinePanel`, `TimelineRuler`, `TimelineTrackLane` e `TrackHeaderPanel` pelos tokens de `OaeTheme`.
  - Refatoração da toolbar de `TimelinePanel` e de `TrackHeaderItem::resized()` para `juce::FlexBox`.
- **Pilar 4 — Acabamento `OaeTheme` + `FlexBox`/`Grid` nos Decks Contextuais**:
  - Refatoração do micro-layout de `PlaylistDeck`, `LogicConditionsDeck`, `Spatializer3DDeck` e `EffectChainDeck` para utilizar `juce::FlexBox` e `juce::Grid`, eliminando dimensões fixas rígidas (`setSize(940, 214)`, `removeFromRight(160)`).
  - Unificação dos tokens de cor restantes com `OaeTheme` nos quatro decks.

## Capabilities

### New Capabilities
<!-- Nenhuma nova capacidade necessária; todos os requisitos pertencem a studio-shell. -->

### Modified Capabilities
- `studio-shell`: Adiciona requisitos observáveis para navegação por *breadcrumb* em eventos aninhados na Timeline, renderização vetorial e edição direta por arraste em curvas de automação da Timeline, e adoção integral de `OaeTheme` e `FlexBox`/`Grid` na Timeline, cabeçalhos de pista e decks contextuais.

## Impact

- **Código Afetado**:
  - `docs/UI_GUIDELINES.md` e `studio/Source/UI/Theme.h` (novos tokens semânticos de clipes e curvas).
  - `studio/Source/UI/TimelinePanel.h/.cpp`, `studio/Source/UI/TimelineRuler.h/.cpp`, `studio/Source/UI/TimelineTrackLane.h/.cpp`, `studio/Source/UI/TrackHeaderPanel.h/.cpp`.
  - `studio/Source/UI/AutomationCurveEditor.h`, `studio/Source/UI/AhdsrCurveEditor.h`.
  - `studio/Source/UI/PlaylistDeck.h/.cpp`, `studio/Source/UI/LogicConditionsDeck.h/.cpp`, `studio/Source/UI/Spatializer3DDeck.h/.cpp`, `studio/Source/UI/EffectChainDeck.h/.cpp`.
  - `studio/Source/Main.cpp` (novos testes de fumaça em `run_smoke_timeline()` cobrindo breadcrumb, arraste em `AutomationLaneComponent` e responsividade de layout).
- **Dependências / Build**: Exclusivo da camada `studio` (JUCE 8). Nenhum impacto no motor nativo C/C++, ABI C, compilador `oae_bankc` ou pacote Unity.
