## Context

`AuthoringModel::get_instrument_targets(event_path, command_type)` em `studio/Source/Model/AuthoringModel.cpp` já retorna:
- `get_snapshot_paths()` quando `command_type == "snapshot_on" || command_type == "snapshot_off"`
- `get_event_paths()` sem filtro de ciclo quando `command_type == "stop_event"`
- `get_event_paths()` com prevenção de ciclo quando `command_type == "start_event"`

Entretanto, `InspectorPanel.cpp` (linhas 218–238) atualmente renderiza apenas o `ComboBox` `Command Target` (`inspector_instrument_target`) lendo `instrument.value("command_type", "start_event")`, sem oferecer UI para alterar `command_type`.

## Goals / Non-Goals

**Goals:**
- Permitir selecionar `start_event`, `stop_event`, `snapshot_on` e `snapshot_off` diretamente no `InspectorPanel` para qualquer `Command Instrument`.
- Manter integridade referencial de `command_target` ao trocar de categoria (`event` <-> `snapshot`) e preservar Undo/Redo via `AuthoringModel::modify`.
- Refletir o `command_type` atualizado tanto no Inspector quanto no clipe da timeline.

**Non-Goals:**
- Alterar o menu `+ Add Instrument` para subdividir 4 submenus de comando (manter criação padrão como `start_event` e troca imediata no Inspector evita poluir o popup).
- Modificar o compilador `oae_bankc` ou o runtime `EventInstance`, que já suportam os 4 tipos.

## Decisions

1. **Método `AuthoringModel::set_command_type`:**
   - Assinatura: `bool set_command_type(const std::string& event_path, size_t track_index, size_t instrument_index, const std::string& command_type);`
   - Validar `command_type` entre `{ "start_event", "stop_event", "snapshot_on", "snapshot_off" }` e `kind == "command"`.
   - Obter `targets = get_instrument_targets(event_path, command_type)`. Se `targets` estiver vazio, retornar `false` (ou manter `command_target` vazio apenas se não houver snapshot/evento válido — preferir exigir `!targets.empty()` para não gerar JSON inválido no `oae_bankc`, exibindo combo desabilitado/aviso se não houver snapshots).
   - Se o `command_target` atual existir em `targets`, preservá-lo; caso contrário, selecionar `targets.front()`.
   - Aplicar via `modify(...)` para registrar transação única no stack de Undo/Redo.

2. **UI no `InspectorPanel`:**
   - Quando `kind == "command"`, inserir `ComboBox` com ID `"inspector_command_type"` e itens `"start_event"`, `"stop_event"`, `"snapshot_on"`, `"snapshot_off"` logo acima de `Command Target`.
   - No `onChange`, chamar `model_.set_command_type(path, track_index, instrument_index, new_type)`. Como `modify(...)` notifica os listeners do `AuthoringModel`, o `InspectorPanel` recarrega a lista de `Command Target` automaticamente.
   - Se não houver snapshots no projeto ao tentar selecionar `snapshot_on`/`snapshot_off`, ou se `set_command_type` retornar `false`, reverter a seleção do combo para o `command_type` anterior (ou desabilitar opções sem alvo).

3. **Rótulo em `TimelineTrackLane` (`ClipComponent`):**
   - Ler `command_type_` em `ClipComponent::update_from_model()` e exibir `CMD [<command_type>] <alvo>` (ou `COMMAND (<command_type>: <alvo>)`) para distinguir visualmente comandos de start, stop e snapshot.

## Risks / Trade-offs

- **Projeto sem snapshots:** se o usuário tentar mudar para `snapshot_on` sem nenhum snapshot criado no projeto, `oae_bankc` rejeitaria `command_target` vazio. Bloquear a troca quando `targets.empty()` (ou desabilitar `snapshot_on`/`snapshot_off` no combo quando `get_snapshot_paths().empty()`) evita estado inválido no preview em tempo real.
