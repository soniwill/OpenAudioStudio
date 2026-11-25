## 1. Modelo, Parser e Formato `.bank`

- [x] 1.1 Adicionar `InstrumentKind::Scatterer = 6` e configurações de spawner em `native/src/core/studio_model.h`, além de validação/parsing de `kind == "scatterer"` em `native/tools/bankc/project_parser.h`.
- [x] 1.2 Implementar serialização e desserialização retrocompatível de `Scatterer` em `native/src/studio/bank_writer.cpp` e `native/src/studio/bank_reader.cpp`, com teste de round-trip em `native/tests/bank_compiler.cpp`.

## 2. Scheduler e Dispersão Espacial em Runtime

- [x] 2.1 Implementar `ActiveScattererState`, agendamento estocástico via `PCG32`, controle de `polyphony`/`total_sounds` e limpeza em `EventInstance` (`native/src/core/event_instance.h` e `event_instance.cpp`).
- [x] 2.2 Aplicar dispersão espacial 2D determinística (panning estéreo + atenuação proporcional em `[min_scatter_distance, max_scatter_distance]`) por grão disparado e adicionar testes CTest em `native/tests/studio_events.cpp` validando com `./scripts/verify.ps1 -Layer native`.

## 3. Autoria no OpenAudio Studio

- [x] 3.1 Adicionar suporte a `kind == "scatterer"` e `set_scatterer_properties(...)` em `studio/Source/Model/AuthoringModel.h` e `AuthoringModel.cpp` (incluindo `PlaylistDeck.cpp` e remoção segura de samples).
- [x] 3.2 Expor `+ Add Instrument -> Scatterer Instrument`, controles no `InspectorPanel.cpp` e visualização `[SCT]` em `TimelineTrackLane.cpp`.

## 4. Validação Final

- [x] 4.1 Adicionar smoke tests de autoria/timeline/compilação para `Scatterer` em `studio/Source/Main.cpp` e validar com `./scripts/build_studio.ps1 -Config Release` e `./scripts/verify.ps1 -Layer docs`.
