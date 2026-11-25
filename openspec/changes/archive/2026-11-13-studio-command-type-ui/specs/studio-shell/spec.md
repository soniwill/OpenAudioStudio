## ADDED Requirements

### Requirement: Seletor de command_type no Inspector para Command Instrument
Quando um `Command Instrument` estiver selecionado no OpenAudio Studio, o `InspectorPanel` SHALL exibir um seletor **`Command Type`** com as opções `start_event`, `stop_event`, `snapshot_on` e `snapshot_off`, além do seletor **`Command Target`** populado com os alvos válidos para o `command_type` ativo.

#### Scenario: Alternar Command Type entre evento e snapshot
- **WHEN** o usuário altera o **`Command Type`** de `start_event` para `snapshot_on` (ou `snapshot_off`) no Inspector
- **THEN** o modelo atualiza `command_type` do instrumento selecionado com suporte a Undo/Redo
- **AND** o combo **`Command Target`** passa a listar apenas snapshots válidos do projeto via `AuthoringModel::get_instrument_targets(event_path, command_type)`
- **AND** caso o alvo anterior não seja válido para o novo tipo, `command_target` é ajustado para um alvo válido (ou preservado se continuar válido, como na troca entre `start_event` e `stop_event`)

#### Scenario: Alternar para stop_event sem bloqueio de ciclo
- **WHEN** o usuário altera o **`Command Type`** para `stop_event`
- **THEN** o combo **`Command Target`** lista os eventos do projeto sem aplicar a restrição de ciclo de `start_event`

### Requirement: Identificação do command_type na timeline
Um `ClipComponent` do tipo `command` na timeline SHALL indicar seu `command_type` juntamente com o `command_target` no texto exibido no clipe.

#### Scenario: Clipe command exibe tipo e alvo
- **WHEN** um `Command Instrument` configurado como `snapshot_on` com alvo `snapshot:/pause_menu` é renderizado em `TimelineTrackLane`
- **THEN** o rótulo do clipe identifica o tipo `snapshot_on` e o alvo correspondente
