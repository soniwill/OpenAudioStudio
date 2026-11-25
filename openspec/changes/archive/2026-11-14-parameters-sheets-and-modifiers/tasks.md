# Tarefas — Etapa 2 (`parameters-sheets-and-modifiers`)

- [x] 1. Validar contrato OpenSpec e atualizar `docs/specs/project-v1.schema.proposed.json` (`sheet_parameter`, `param_min`, `param_max` em instrumentos e `seek_speed`, `seek_speed_down`, `velocity`, `hold_during_playback` em parâmetros `continuous`/`discrete`/`labeled`).
- [x] 2. Implementar `velocity` e `hold_during_playback` em `ParameterDefinition`, `ParameterInstance` e `EventInstance` (`native/src/core/studio_model.h`, `native/src/core/event_instance.h`, `native/src/core/event_instance.cpp`).
- [x] 3. Serializar chunks opcionais `PSHT` e `PMOD` no formato `.bank` e estender `oae_bankc` (`native/src/studio/bank_parm_codec.h`, `bank_writer.cpp`, `bank_reader.cpp`, `native/tools/bankc/project_parser.h`).
- [x] 4. Estender `AuthoringModel` e `InspectorPanel` para editar parâmetros locais/globais completos (`Discrete`, `Labeled`, `Seek Speed`, `Velocity`, `Hold`), faixas de Parameter Sheet em instrumentos e `user_props` / `notes` / `tags` em eventos (`studio/Source/Model/AuthoringModel.*`, `studio/Source/UI/InspectorPanel.cpp`).
- [x] 5. Adicionar testes automatizados em `native/tests/parameter_conditions.cpp`, `native/tests/bank_compiler.cpp` e smoke test `--smoke-authoring` em `studio/Source/Main.cpp`.
- [x] 6. Executar `scripts/verify.ps1 -Layer native`, `scripts/verify.ps1 -Layer studio` e `scripts/verify.ps1 -Layer docs`, e sincronizar/arquivar a change atualizando `docs/ROADMAP.md` e `docs/ai/memory.md`.
