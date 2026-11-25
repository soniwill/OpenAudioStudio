# Design

## Context

Consulte [proposal.md](proposal.md) para motivação e escopo. O motor nativo (`EventInstance`) e o compilador/leitor de banco (`bank_writer.cpp` / `bank_reader.cpp`) já suportam todos os tipos de `logic_items` (`marker`, `tempo`, `loop_region` com `until_key_off`, `sustain_point`, `transition_marker` e `transition_region` com `crossfade_ms`, `quantization` e `Parameter Conditions`). No Studio, `TimelineRuler` e `LogicConditionsDeck` já renderizam e editam parte desses itens, mas faltam interações diretas na régua, criação de `transition_region`, controles de `until_key_off`, `crossfade_ms`, edição de `marker`/`tempo` e agrupamento de Undo por gesto.

## Goals / Non-Goals

**Goals:**
- Fechar paridade de autoria e UX para todos os tipos de `logic_items` suportados pelo runtime sem alterar o formato `.bank`.
- Garantir Undo/Redo limpo por gesto tanto na `TimelineRuler` quanto nos sliders do `LogicConditionsDeck`.
- Validar criação/edição/compilação em smoke test dedicado no Studio.

**Non-Goals:**
- Alterar o formato `.bank` ou a semântica de execução de `EventInstance`.
- Implementar curvas de tempo contínuas (apenas marcadores discretos de `tempo`).

## Decisions

1. **Preview local de drag/resize na `TimelineRuler` antes de `model_.modify(...)`:**
   - Durante `mouseDrag`, a `TimelineRuler` mantém um estado temporário (`drag_preview_start_ms_`, `drag_preview_length_ms_`, `drag_mode_` = `Move` | `ResizeLeft` | `ResizeRight`) e redesenha imediatamente sem poluir o histórico de Undo nem disparar rebuilds de prévia a cada pixel.
   - No `mouseUp`, aplica uma única chamada `model_.modify(...)` (respeitando `TimelineSnap` quando `Shift` não estiver pressionado).
   - *Alternativa descartada:* chamar `begin_gesture()` + `model_.modify()` a cada `mouseDrag` — causaria rebuilds de `LogicConditionsDeck` e sobrecarga desnecessária de JSON durante o arraste.

2. **Hit-testing de alças laterais em regiões (`loop_region` e `transition_region`):**
   - Zona de alça de `6 px` nas bordas esquerda/direita de regiões com largura visual `>= 16 px`; o miolo faz `Move`. Cursor muda para `LeftRightResizeCursor` em `mouseMove` sobre as bordas.

3. **Renomeação segura de `Marker` e edição de `Tempo` no `AuthoringModel`:**
   - Adicionar helpers explícitos em `AuthoringModel` (`rename_marker` ou atualização atômica em `modify`) que, ao renomear um `marker`, atualizam também qualquer `target_marker` igual no mesmo evento dentro da mesma transação de Undo.
   - Para `tempo`, validar `bpm` (`[20.0, 300.0]`), `ts_upper` (`[1, 16]`) e `ts_lower` (`{2, 4, 8, 16}`).

4. **Undo por gesto nos sliders do `LogicConditionsDeck`:**
   - Conectar `onDragStart` -> `model_.begin_gesture(...)` e `onDragEnd` -> `model_.end_gesture()` nos sliders (`position_`, `length_`, `repeats_`, `probability_`, `minimum_`, `maximum_`, `crossfade_`, `bpm_`), alinhado ao padrão já usado no `InspectorPanel`.

## Risks / Trade-offs

- **[Densidade visual no `LogicConditionsDeck`]** → Exibir controles contextuais (`until_key_off` só para `loop_region`, `crossfade_` e `target_`/`quantization_` só para transições, `marker_name_` só para `marker`, `bpm_`/`ts_upper_`/`ts_lower_` só para `tempo`) para manter o layout limpo e sem sobreposição.
- **[Sobreposição visual na `TimelineRuler`]** → Usar faixas/cores distintas por tipo (já existentes: azul para Marker, verde para Tempo, âmbar translúcido para Loop, coral para Transition, roxo para Sustain) e truncamento elegante dos rótulos.
