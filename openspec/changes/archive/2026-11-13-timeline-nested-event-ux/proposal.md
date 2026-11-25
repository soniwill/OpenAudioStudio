## Why

Na timeline do OpenAudio Studio, clipes adicionados na mesma pista são desenhados em uma única faixa vertical com preenchimento opaco. Quando um `Event Instrument` sobrepõe um `Single Instrument` existente na mesma pista, ele cobre o waveform do instrumento pai, além de não renderizar um waveform representativo do evento filho e truncar o título em `EVEN...`.

## What Changes

- Adicionar distribuição automática em sub-lanes verticais em `TimelineTrackLane` sempre que clipes na mesma pista se sobrepuserem no tempo (`[offset_ms, offset_ms + length_ms)`), mantendo a altura cheia quando não houver sobreposição.
- Melhorar a renderização visual de `Event Instrument` em `ClipComponent` com badge curto `[EVT] <nome>`, tooltip/título legível e renderização de waveform resumo dos instrumentos `Single` do evento alvo (com fallback visual quando o evento alvo não possuir áudio direto).
- Adicionar navegação por duplo-clique em clipes `Event Instrument` para selecionar o evento filho alvo no `AuthoringModel`.
- Cobrir o comportamento de layout em sub-lanes, renderização/resolução do evento alvo e navegação por duplo-clique nos smoke tests do Studio.

## Capabilities

### New Capabilities
- `studio-shell`: Requisitos de apresentação e navegação na timeline do Studio para clipes sobrepostos em sub-lanes e `Event Instrument` (badge `[EVT]`, waveform resumo do filho e duplo-clique para abrir o alvo).

### Modified Capabilities
- Nenhum.

## Impact

- `studio/Source/UI/TimelineTrackLane.h` e `studio/Source/UI/TimelineTrackLane.cpp`: cálculo de sub-lanes na pista, renderização de `ClipComponent` para `kind == "event"` e tratamento de `mouseDoubleClick`.
- `studio/Source/Main.cpp` (smoke tests `--smoke-timeline`): validação automatizada de sobreposição em sub-lanes e duplo-clique em `Event Instrument`.
- Sem alterações na API C, formato `.bank` ou runtime nativo.
