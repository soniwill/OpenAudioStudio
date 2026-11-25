## 1. Sub-lanes automáticas em TimelineTrackLane

- [x] 1.1 Implementar alocação greedy de sub-lanes por intervalo em `TimelineTrackLane::resized()` (`studio/Source/UI/TimelineTrackLane.cpp`), dividindo a altura disponível entre os clipes sobrepostos e mantendo altura cheia quando não houver sobreposição.
- [x] 1.2 Garantir compatibilidade de interação (`mouseDown`, `mouseDrag`, seleção e menu de contexto) dos `ClipComponent` posicionados em sub-lanes.

## 2. Visual e navegação de Event Instrument

- [x] 2.1 Atualizar `ClipComponent::paint()` em `studio/Source/UI/TimelineTrackLane.cpp` para renderizar `[EVT] <nome-do-evento>` e o waveform resumo do evento filho via `WaveformCache` (com fallback limpo quando não houver sample direto).
- [x] 2.2 Implementar `ClipComponent::mouseDoubleClick()` em `studio/Source/UI/TimelineTrackLane.h` e `TimelineTrackLane.cpp` para selecionar o `target_event` no `AuthoringModel`.

## 3. Validação

- [x] 3.1 Adicionar verificação em `--smoke-timeline` (`studio/Source/Main.cpp`) cobrindo sub-lanes em clipes sobrepostos e navegação por duplo-clique em `Event Instrument`.
- [x] 3.2 Validar com `./scripts/build_studio.ps1 -Config Release` e `./scripts/verify.ps1 -Layer docs`.
