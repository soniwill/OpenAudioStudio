# Tasks

## 1. Modelo de Autoria (`AuthoringModel`)

- [x] 1.1 Adicionar e validar em `studio/Source/Model/AuthoringModel.h/.cpp` helpers para `add_transition_region`, `rename_marker` (sincronizando referências `target_marker` no mesmo evento) e atualização de `tempo` (`bpm`, `ts_upper`, `ts_lower`) com suporte a Undo/Redo; verificar compilação sem warnings no build Release do Studio.

## 2. Manipulação Visual na `TimelineRuler`

- [x] 2.1 Implementar preview visual ao vivo durante `mouseDrag` e alças laterais de redimensionamento (`ResizeLeft`/`ResizeRight` com cursor apropriado) para `loop_region` e `transition_region` em `studio/Source/UI/TimelineRuler.h/.cpp`, aplicando a mudança ao `AuthoringModel` no `mouseUp` em um único passo de Undo/Redo; verificar por inspeção e smoke de UI.
- [x] 2.2 Atualizar `TimelineRuler::paint`, `show_context_menu` e tratamento de tecla `Delete` (`TimelineRuler` / `TimelinePanel`) para exibir rótulos informativos (alvo `-> Marker`, quantização `Beat`/`Bar`, repetições `∞`/`Nx`/`KeyOff`, badge de condição) e permitir criar todos os tipos (`Marker`, `Tempo Marker`, `Loop Region`, `Transition Marker`, `Transition Region`, `Sustain Point`) e remover itens lógicos; verificar build e comportamento nos smokes.

## 3. Controles Completos e Undo por Gesto no `LogicConditionsDeck`

- [x] 3.1 Adicionar ao `studio/Source/UI/LogicConditionsDeck.h/.cpp` criação de `Transition Region`, toggle `Until Key Off` para `loop_region`, slider `Crossfade (ms)` para transições, edição de nome de `Marker` e edição de `Tempo` (`BPM`, `ts_upper`, `ts_lower`), além de `begin_gesture`/`end_gesture` nos sliders; verificar compilação e operação contextual por tipo selecionado.

## 4. Verificação, Manual e Memória

- [x] 4.1 Expandir os smoke tests do Studio em `studio/Source/Main.cpp` para cobrir criação/edição/resize/undo de `transition_region`, `until_key_off`, `crossfade_ms`, renomeação referencial de `marker`, edição de `tempo`, `Parameter Conditions` e compilação limpa via `oae_bankc`; executar `scripts/verify.ps1 -Layer studio` e confirmar `RESULT: OK`.
- [x] 4.2 Atualizar o capítulo de Timeline/Lógica em `studio/Resources/manual/`, apontar `T09` no legado para o spec canônico quando aplicável e registrar o resultado em `docs/ai/memory.md`; executar `scripts/verify.ps1 -Layer docs` e `openspec validate "timeline-logic-ui" --strict`.
