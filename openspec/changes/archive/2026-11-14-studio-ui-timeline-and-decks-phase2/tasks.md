# Tasks

## 1. Tokens Semânticos de Timeline/Curvas e Governança

- [x] 1.1 Estender `studio/Source/UI/Theme.h` e atualizar `docs/UI_GUIDELINES.md` com tokens semânticos para categorias de clipes (`Single`, `Multi`, `Event`, `Command`, `Scatterer`, `Programmer`), linhas de grade da Timeline/Régua e opacidades de preenchimento de curvas (*ribbon fill*).
- [x] 1.2 Substituir as cores hexadecimais literais em `TimelinePanel.cpp`, `TimelineRuler.cpp`, `TimelineTrackLane.cpp` e `TrackHeaderPanel.cpp` pelos tokens de `OaeTheme`, verificando a compilação do alvo `oae_studio`.

## 2. Breadcrumb de Nested Events e Micro-Layout da Timeline/Headers

- [x] 2.1 Implementar a trilha de navegação pai -> filho e os controles de *breadcrumb* (botão voltar + chips clicáveis por nível hierárquico) na toolbar superior de `TimelinePanel.h/.cpp` em integração com `TimelineTrackLane.cpp` e `AuthoringModel`.
- [x] 2.2 Refatorar `TimelinePanel::resized()` (toolbar superior) e `TrackHeaderItem::resized()` em `TrackHeaderPanel.cpp` para utilizar `juce::FlexBox`, garantindo adaptação fluida sem sobreposição em larguras compactas.
- [x] 2.3 Adicionar teste de fumaça em `studio/Source/Main.cpp` (`run_smoke_timeline()`) verificando a navegação de evento pai para evento filho (`[EVT]`), a atualização do *breadcrumb* e o retorno ao evento pai pelo clique no *breadcrumb*.

## 3. Curvas Vetoriais (Bézier + Ribbon Fill) e Arraste Direto na Timeline

- [x] 3.1 Atualizar `AutomationLaneComponent` (`TimelineTrackLane.cpp`), `AutomationCurveEditor.h`, `AhdsrCurveEditor.h` e `EqPlot` (`EffectChainDeck.cpp`) para desenhar curvas suaves respeitando o `shape` (`linear`, `hold`, `ease_in`, `ease_out`, `s_curve`), preenchimento em gradiente translúcido sob a curva, grade sutil e nós com halo vetorial.
- [x] 3.2 Implementar `mouseDown`, `mouseDrag` e `mouseUp` em `AutomationLaneComponent` (`TimelineTrackLane.h/.cpp`) permitindo arrastar pontos existentes na pista da Timeline com leitura de valor e registro de passo único de Undo/Redo no `AuthoringModel`.
- [x] 3.3 Adicionar teste de fumaça em `studio/Source/Main.cpp` (`run_smoke_timeline()`) cobrindo a criação, o arraste de ponto e o Undo/Redo na `AutomationLaneComponent`.

## 4. Micro-Layout FlexBox/Grid e Tokens nos Decks Contextuais

- [x] 4.1 Refatorar `PlaylistDeck.cpp`, `LogicConditionsDeck.cpp`, `Spatializer3DDeck.cpp` e `EffectChainDeck.cpp` para substituir fatiamentos de tamanho fixo por `juce::FlexBox` e `juce::Grid` e migrar cores literais remanescentes para `OaeTheme`.
- [x] 4.2 Executar a suíte completa de verificação do Studio e da documentação (`powershell -NoProfile -File scripts/verify.ps1 -Layer studio` e `powershell -NoProfile -File scripts/verify.ps1 -Layer docs`) e atualizar `docs/ai/memory.md`.
