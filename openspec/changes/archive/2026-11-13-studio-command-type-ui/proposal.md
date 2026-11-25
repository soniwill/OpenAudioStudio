## Why

O runtime nativo (`EventInstance`) já executa todos os quatro tipos de `Command Instrument` (`start_event`, `stop_event`, `snapshot_on` e `snapshot_off`) e `AuthoringModel::get_instrument_targets()` já calcula os alvos válidos para cada tipo. No entanto, `InspectorPanel` no OpenAudio Studio hoje só expõe o combo `Command Target` assumindo `start_event` na criação, sem permitir alternar `command_type` pela interface gráfica.

## What Changes

- Adicionar suporte no `AuthoringModel` para alterar `command_type` de um `Command Instrument` existente (`start_event`, `stop_event`, `snapshot_on`, `snapshot_off`) com atualização automática de `command_target` para um alvo válido do novo tipo e suporte completo a Undo/Redo.
- Expor no `InspectorPanel` um seletor **`Command Type`** (`inspector_command_type`) quando o instrumento selecionado for `kind == "command"`, sincronizado com o combo **`Command Target`** (`inspector_instrument_target`).
- Exibir o tipo de comando no título/rótulo do clipe `command` na timeline (`TimelineTrackLane`) para identificação imediata.
- Cobrir a troca de `command_type`, atualização de alvos válidos (eventos vs snapshots) e Undo/Redo nos smoke tests do Studio.

## Capabilities

### New Capabilities
- `studio-shell`: Requisitos de autoria no Inspector e timeline para seleção e visualização de `command_type` (`start_event`, `stop_event`, `snapshot_on`, `snapshot_off`) em `Command Instrument`.

### Modified Capabilities
- Nenhum.

## Impact

- `studio/Source/Model/AuthoringModel.h` e `studio/Source/Model/AuthoringModel.cpp`: novo método `set_command_type(...)` com validação de tipo e reconciliação de `command_target`.
- `studio/Source/UI/InspectorPanel.cpp`: combo `Command Type` acima de `Command Target` para instrumentos `command`.
- `studio/Source/UI/TimelineTrackLane.h` e `studio/Source/UI/TimelineTrackLane.cpp`: exibição do `command_type` no rótulo do clipe `command`.
- `studio/Source/Main.cpp`: asserções em `--smoke-authoring` (ou `--smoke-timeline`) para alternância de `command_type`, alvos válidos e Undo/Redo.
- Sem alterações no runtime nativo, ABI C ou formato `.bank`.
