# Design Técnico: Espacialização 3D Nativa, Built-in Parameters e Envelopment

## Context
Atualmente `EventInstance` controla apenas `volume_`, `pitch_` e `pan_` escalares, e tanto `Spatializer3DDeck` (Studio) quanto `StudioEventEmitter` (Unity) calculam atenuação e pan manualmente no espaço do cliente. Para paridade com `EVT-02`, `PAR-02` e `MIX-02`, o cálculo 3D precisa residir no runtime nativo `System`/`EventInstance` atrás da C API (`oae_audio.h`), alimentando continuamente tanto o DSP de ganho/pan quanto os Built-in Parameters avaliados por condições, automações e Parameter Sheets.

## Goals / Non-Goals
- **Goals**:
  - Preservar integralmente `oae_struct_sizes` existente e compatibilidade ABI com `NativeStructSizeTests` e `T14a`.
  - Adotar referencial canhoto consistente com Unity (`+X` direita, `+Y` cima, `+Z` frente) com base ortonormalizada a partir de `forward` e `up`.
  - Calcular `Distance` `[0, inf)`, `Direction` `[-180, 180]`, `Elevation` `[-90, 90]`, `EventConeAngle` `[0, 180]` e `Speed` `[0, inf)` sem alocações por frame.
  - Garantir que `spatial_gain_` e `spatial_pan_` não sejam multiplicados/somados em duplicidade nas vozes existentes nem sobrescrevam o `volume_`/`pan_` de base ao ativar/desativar o spatializer.
- **Non-Goals**:
  - HRTF binaural completo fica fora do escopo desta etapa (panning estéreo + envelopment com `Sound Size` e `Min Extent`).

## Decisions
1. **Structs C ABI e Compatibilidade (`oae_audio.h`)**:
   - Adicionar `oae_rolloff_type`, `oae_vec3`, `oae_3d_attributes` e `oae_spatializer_settings` e 6 funções `oae_system_*_listener_3d_attributes` / `oae_event_instance_*_3d_attributes` / `oae_event_instance_*_spatializer_settings`.
   - Não alterar o layout de `oae_struct_sizes` para evitar quebra de código cliente compilado contra a v1 existente.
2. **Base Ortonormal do Listener e Built-in 3D Parameters (`EventInstance::refresh_3d_state`)**:
   - Listener padrão: `position = {0,0,0}`, `velocity = {0,0,0}`, `forward = {0,0,1}`, `up = {0,1,0}`.
   - Cross product canhoto `right = normalize(cross(up, forward))` garante que `{0,1,0} x {0,0,1} = {1,0,0}`; `up = cross(forward, right)`.
   - `Direction = atan2(dot(dir, right), dot(dir, forward))` em graus; `Elevation = asin(clamp(dot(dir, up), -1, 1))` em graus; `EventConeAngle = acos(clamp(dot(emitter_forward, -dir), -1, 1))` em graus; `Speed = length(emitter_velocity - listener_velocity)`.
   - Os valores Built-in 3D sincronizam qualquer entrada homônima em `local_parameters_` (respeitando `hold_during_playback`) e também ficam disponíveis via `try_get_builtin_parameter` como fallback em `get_parameter`, `evaluate_condition`, `evaluate_parameter_sheets` e automações, mesmo sem declaração explícita no modelo do evento.
3. **Separação entre Ganho/Pan da Voz e Espacialização da Instância**:
   - `spatial_gain_` e `spatial_pan_` residem em `EventInstance` e são aplicados dinamicamente em `update_channel_parameters()` e `effective_volume()`, enquanto `trigger_instrument()` / `spawn_scatterer_grain()` passam ou armazenam apenas o ganho/pan não espacializado em `ActiveInstVoice` e aplicam a composição final via `update_channel_parameters()`.
   - Na reentrada do ponteiro 3D ou mudança de listener, `System::set_listener_3d_attributes()` notifica todas as `activeInstances_`.

## Risks / Trade-offs
- Em `oae_system_play_sound` / `EventInstance::trigger_instrument()`, armazenar `ActiveInstVoice::volume_db` / `pan` sem contaminar com `spatial_gain_` / `spatial_pan_` exige cuidado para que mudanças subsequentes de posição 3D atualizem todas as vozes já tocando com exatidão. Chamar `update_channel_parameters()` ou manter `base_vol`/`base_pan` livre de `spatial_*` ao popular `active_voices_` resolve esse risco de raiz.
