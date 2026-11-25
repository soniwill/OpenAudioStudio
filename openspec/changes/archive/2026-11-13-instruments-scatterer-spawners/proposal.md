## Why

O `OpenAudioStudio` já suporta `Single`, `Multi`, `Programmer`, `Silence`, `Event` e `Command` instruments em runtime, mas ainda não possui o **Scatterer Instrument** para disparar grãos sonoros recorrentes com intervalo estocástico, limite de polifonia e dispersão espacial (ex.: chuva, passos múltiplos, pássaros, faíscas ou ambiências granulares).

## What Changes

- Adicionar `InstrumentKind::Scatterer` e suas propriedades de spawner (`spawn_interval_min_ms`, `spawn_interval_max_ms`, `polyphony`, `total_sounds`, `min_scatter_distance`, `max_scatter_distance`) ao modelo nativo (`studio_model.h`), parser de projeto (`project_parser.h`) e formato `.bank` com extensão retrocompatível (`bank_writer.cpp` / `bank_reader.cpp`).
- Implementar o scheduler determinístico de `Scatterer` em `EventInstance` usando `PCG32` para sortear entradas da playlist (samples ou eventos aninhados), respeitar `polyphony` e `total_sounds`, e aplicar variação de volume/pitch e espacialização (panning estéreo + atenuação proporcional por distância como fallback 2D).
- Expor autoria completa no OpenAudio Studio (`AuthoringModel`, `InspectorPanel`, `PlaylistDeck` e clipe `[SCT]` em `TimelineTrackLane`).
- Cobrir compilação de banco, scheduler em `EventInstance` (CTest) e autoria/preview no Studio (`--smoke-authoring` / `--smoke-timeline`).

## Capabilities

### New Capabilities
- Nenhum.

### Modified Capabilities
- `instruments`: Adicionar requisitos de execução em runtime de `Scatterer Instruments` (agendamento estocástico determinístico, polifonia, dispersão espacial 2D/3D e playlist de grãos).
- `studio-shell`: Adicionar requisitos de autoria no Inspector, PlaylistDeck e Timeline para `Scatterer Instruments`.

## Impact

- `native/src/core/studio_model.h`, `native/tools/bankc/project_parser.h`, `native/src/studio/bank_writer.cpp`, `native/src/studio/bank_reader.cpp`: modelo, parser JSON e serialização retrocompatível de `Scatterer`.
- `native/src/core/event_instance.h` e `native/src/core/event_instance.cpp`: estado por spawner ativo (`ActiveScattererState`), disparo periódico de grãos e limpeza em `stop()` / saída de região.
- `native/tests/studio_events.cpp` e `native/tests/bank_compiler.cpp`: novos testes CTest de compilação e execução em runtime.
- `studio/Source/Model/AuthoringModel.*`, `studio/Source/UI/InspectorPanel.cpp`, `studio/Source/UI/PlaylistDeck.cpp`, `studio/Source/UI/TimelineTrackLane.cpp`, `studio/Source/Main.cpp`: UI de autoria e smoke tests.
- Modulação dinâmica de `spawn_rate` por parâmetro permanece fora de escopo (planejada junto de `modulation-automation-ui`).
