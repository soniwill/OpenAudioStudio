## 1. Modelo de Autoria (AuthoringModel)

- [x] 1.1 Adicionar `set_command_type(const std::string& event_path, size_t track_index, size_t instrument_index, const std::string& command_type)` em `studio/Source/Model/AuthoringModel.h` e `AuthoringModel.cpp`, validando os quatro tipos (`start_event`, `stop_event`, `snapshot_on`, `snapshot_off`), reconciliando `command_target` com `get_instrument_targets()` e preservando Undo/Redo.

## 2. Interface do Inspector e Timeline

- [x] 2.1 Adicionar o seletor **`Command Type`** (`inspector_command_type`) acima de **`Command Target`** em `studio/Source/UI/InspectorPanel.cpp` para instrumentos `kind == "command"`, desabilitando opções de snapshot quando não houver snapshots no projeto e atualizando `Command Target` automaticamente ao trocar o tipo.
- [x] 2.2 Atualizar `ClipComponent` em `studio/Source/UI/TimelineTrackLane.h` e `TimelineTrackLane.cpp` para armazenar e exibir `command_type_` no rótulo de clipes `command`.

## 3. Validação

- [x] 3.1 Adicionar asserções nos smoke tests do Studio (`studio/Source/Main.cpp`) cobrindo troca de `command_type` (`start_event` -> `stop_event` -> `snapshot_on` -> `snapshot_off`), atualização de `command_target`, Undo/Redo e compilação de preview.
- [x] 3.2 Validar com `./scripts/build_studio.ps1 -Config Release` e `./scripts/verify.ps1 -Layer docs`.
