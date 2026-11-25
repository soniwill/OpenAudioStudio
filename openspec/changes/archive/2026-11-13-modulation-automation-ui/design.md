# Design

## Context

Veja `proposal.md` para motivação e escopo. O runtime (`native/src/core/studio_model.h`, `event_instance.cpp`) e `oae_bankc` já implementam `Modulator` (`random`, `ahdsr`) e `Automation` / `EffectAutomation` (`1..32` pontos, 5 formas de segmento). A UI atual em `TimelineTrackLane` edita apenas a primeira automação de faixa na subfaixa `A`, sem seção de modulação nem editor dedicado de curvas por parâmetro no `InspectorPanel`.

## Goals / Non-Goals

**Goals:**
- Expor `RANDOM` e `AHDSR` (com gráfico interativo) no `InspectorPanel` para Eventos e Instrumentos.
- Expor editor visual de curvas por parâmetro (`Volume`, `Pitch`, `Pan` e `EffectAutomation`) no `InspectorPanel`.
- Garantir que arrastes contínuos de sliders ou pontos no gráfico usem `begin_gesture()` / `end_gesture()` sem destruir componentes ativos durante o arraste.

**Non-Goals:**
- Não adicionar `LFO` ou `Seek` ao runtime/schema nesta change.
- Não alterar `TimelineTrackLane` para além da atualização automática via `on_project_changed()`.

## Decisions

1. **Editor de Curvas e Seção Modulation hospedados no `InspectorPanel`**
   - Em vez de criar uma nova aba no `ContextualBottomDeck`, integrar a seção **Modulation** e o **Parameter Automation Curve Editor** diretamente no `InspectorPanel` (com altura dinâmica do `view->setSize(...)`).
   - **Racional:** mantém a edição contextual ao item selecionado (Evento, Faixa, Instrumento ou Bus/Efeito) sem aumentar a complexidade de navegação do deck inferior.

2. **Componentes visuais reutilizáveis e isolamento de gestos**
   - Criar componentes JUCE dedicados e enxutos (ex.: `AhdsrCurveEditor` e `AutomationCurveEditor`) que recebem callbacks de início/fim de gesto (`begin_gesture`/`end_gesture`) e de mutação, evitando `rebuild()` destrutivo do `InspectorPanel` enquanto `edit_depth_ > 0` (padrão já adotado em `InspectorPanel` com `refresh_pending_`).

3. **Normalização centralizada no `AuthoringModel`**
   - Centralizar adição, atualização, remoção e ordenação de `modulators` e `automations` / `effect_automations` no `AuthoringModel` para que UI e smokes/testes compartilhem exatamente as mesmas invariantes (`range`, tempos `>= 0`, `1..32` pontos ordenados em `x`).

## Risks / Trade-offs

- **[Risco] Reconstrução do `InspectorPanel` durante arraste no gráfico** -> **Mitigação:** usar `begin_gesture()`/`end_gesture()` e respeitar `edit_depth_` + `refresh_pending_` como nos `GestureSlider`s existentes.
- **[Risco] Pontos com `x` duplicado ou fora de ordem causarem falha no `oae_bankc`** -> **Mitigação:** ordenar e impor espaçamento mínimo estrito em `x` ao inserir/mover pontos no `AuthoringModel`.
