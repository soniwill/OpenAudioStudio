# Design Técnico: Parameter Sheets, Modificadores de Parâmetro e User Properties

## 1. Formato `.bank` Retrocompatível (Chunks Opcionais `PSHT` e `PMOD`)
Para manter compatibilidade binária absoluta com bancos `.bank` v1 já existentes e evitar alterações nas chunks obrigatórias `EVTS` e `PARM`:
- **`PSHT` (Parameter Sheet bindings)**: escrita somente quando ao menos um instrumento possui `!inst.sheet_parameter.empty()`. Layout sequencial: `uint32_t count` seguido de `count` registros `{ uint32_t event_idx, uint32_t track_idx, uint32_t inst_idx, uint32_t str_idx_sheet_param, double param_min, double param_max }`.
- **`PMOD` (Parameter Modifiers)**: escrita somente quando ao menos um parâmetro global ou local possui `velocity != 0.0f` ou `hold_during_playback == true`. Layout sequencial:
  - `uint32_t global_count` + registros `{ uint32_t global_idx, float velocity, bool hold_during_playback }`
  - `uint32_t local_count` + registros `{ uint32_t event_idx, uint32_t param_idx, float velocity, bool hold_during_playback }`

## 2. Semântica de `velocity` e `hold_during_playback`
- **Velocity**: Em `ParameterInstance::advance_time(delta_seconds)`, quando `def_.velocity != 0.0f`, o alvo `target_value_` desloca-se `velocity * delta_seconds` limitado a `[min_value, max_value]`. Caso não haja seek speed limitante (`seek_speed <= 0 && seek_speed_down <= 0`), `current_value_` acompanha imediatamente (snapping em `std::round` apenas se `Discrete`/`Labeled` sem progressão fracionária contínua — mantemos acumulação contínua em `target_value_` e `std::round` somente em `set_value` ou leitura discreta? Melhor: se `Discrete`/`Labeled` possui `velocity != 0`, `target_value_` acumula frações continuamente para não truncar incrementos pequenos por frame, enquanto `current_value_` aplica `std::round` se necessário ou acompanha `target_value_` dentro de `[min, max]` para precisão sub-frame). Pensando bem, aplicar `std::round` em `current_value_` apenas quando `def_.kind != Continuous` garante que valores discretos/labeled continuem inteiros enquanto `target_value_` acumula `velocity * delta_sec` sem perder precisão! Vamos validar isso nos testes.
- **Hold value during playback**:
  - Para **parâmetros locais**: `EventInstance::set_parameter` e `set_parameter_with_label` ignoram atualizações quando `is_active() && def.hold_during_playback`.
  - Para **parâmetros globais**: Como um parâmetro global é compartilhado no `System` por várias instâncias, congelar no `System` bloquearia novas instâncias. Portanto, no momento de `EventInstance::start()`, cada parâmetro global presente nos `ModelSet`s com `hold_during_playback == true` tem seu `current_value()` congelado em `held_global_parameters_` na instância ativa, e `get_parameter` / `evaluate_condition` / `automations` leem o snapshot da instância enquanto `is_active()`. Quando a instância para (`stop()`), `held_global_parameters_.clear()` libera o snapshot para o próximo `start()`.

## 3. Studio Authoring (`AuthoringModel` e `InspectorPanel`)
- `create_param_editor` passa a editar parâmetros globais completos (`kind`, `min`, `max`, `labels`, `default_value`, `seek_speed`, `seek_speed_down`, `velocity`, `hold_during_playback`).
- `create_event_editor` ganha:
  - Por instrumento (exceto `command`/`silence`): `Sheet Param`, `Param Min`, `Param Max`.
  - Seção `LOCAL PARAMETERS` com CRUD completo e suporte aos mesmos modificadores.
  - Seção `USER PROPERTIES & NOTES` permitindo editar `notes`, `tags` e chaves arbitrárias em `user_props`, mantendo preservadas chaves internas (`track_*_mute`, `track_*_solo`).
