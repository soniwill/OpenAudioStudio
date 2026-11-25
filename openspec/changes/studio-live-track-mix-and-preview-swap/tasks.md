# Tasks

## 1. Modelo de autoria e ponte de mixagem de faixas em tempo real

- [x] 1.1 Em `studio/Source/Model/AuthoringModel.h` e `studio/Source/Model/AuthoringModel.cpp`, adicionar `modify_audition(...)` (ou parâmetro `affects_compiled_bank = false` em `modify`) usado por `set_track_muted` e `set_track_solo` sem incrementar `content_revision_`, adicionar getters de faixa (`get_track_count`, `get_track_volume`, `get_track_pan`, `get_track_output_track`) e remapear as chaves `track_<idx>_mute` / `track_<idx>_solo` em `user_props` nos métodos `remove_track`, `duplicate_track` e `reorder_track`.
  - **Camada:** Studio (Model)
  - **Aceite:** `set_track_muted`/`set_track_solo` preservam Undo/Redo e disparam `on_project_changed()` sem alterar `get_content_revision()`; reordenar/remover/duplicar faixas preserva `is_track_muted`/`is_track_solo` nas faixas corretas (validado em `--smoke-timeline` e `--smoke-transport-mixer`).

- [x] 1.2 Em `studio/Source/StudioEngine.h` e `studio/Source/StudioEngine.cpp`, adicionar `get_instance_track_bus(oae_event_instance instance, uint32_t track_index, oae_bus* out_bus)` chamando `oae_event_instance_get_track_bus`.
  - **Camada:** Studio (Engine wrapper)
  - **Aceite:** compilação Release sem avisos e retorno `OAE_OK` com handle válido para faixas de instâncias em reprodução em `--smoke-transport-mixer`.

- [x] 1.3 Em `studio/Source/Transport/AuditionTransport.h` e `studio/Source/Transport/AuditionTransport.cpp`, fazer `AuditionTransport` implementar `AuthoringModel::Listener` (registrando/desregistrando no construtor/destrutor) e aplicar `sync_active_track_mix()` imediatamente após `oae_event_instance_start` em `play()`, `resume()`, reinício de loop em `update_tick()` e em `on_project_changed()`, aplicando `oae_bus_set_volume`, `oae_bus_set_pan` e `oae_bus_set_mute` com precedência `Mute` vence `Solo` (respeitando ancestralidade `output_track`).
  - **Camada:** Studio (Transport)
  - **Aceite:** durante reprodução ativa, alterar `M`, `S`, `Volume` ou `Pan` (incluindo via Undo/Redo) reflete imediatamente nos valores lidos por `oae_bus_get_mute`, `oae_bus_get_volume` e `oae_bus_get_pan` nos buses de faixa em `--smoke-transport-mixer`.

## 2. Hot-swap seguro de banco de prévia com streaming

- [x] 2.1 Em `studio/Source/Transport/PreviewRebuilder.h` e `studio/Source/Transport/PreviewRebuilder.cpp`, implementar compilação em slots alternados (`build/preview_a.bank` e `build/preview_b.bank`), ignorar `on_project_changed()` quando `loaded_preview_bank_ != 0 && model_.get_content_revision() == last_rebuilt_revision_`, adiar `timerCallback()` enquanto `model_.is_in_gesture()` for `true`, executar `transport_.stop(true)` + `transport_.stop_all_snapshots()` + `engine_.unload_bank(...)` + `engine_.update()` antes da troca de slot em `apply_compiled_bank` e limpar em modo best-effort arquivos obsoletos/descartados em `apply_compiled_bank` e `unload_preview_bank`. Atualizar `.gitignore` para `/example_project/build/preview*.bank*`.
  - **Camada:** Studio (Preview / Build)
  - **Aceite:** recompilar repetidamente um projeto com sample `"stream": true` em reprodução ativa retorna sucesso (`Preview In Sync`, zero erro `Atomic replacement failed`) e mantém `Mute`/`Solo`/`Volume`/`Pan` reaplicados após o hot-swap (validado em `--smoke-transport-mixer`).

## 3. Identificadores MCP e sincronia visual em `TrackHeaderPanel`

- [x] 3.1 Em `studio/Source/UI/TrackHeaderPanel.cpp` (e `.h` se necessário), registrar e atualizar em `set_track_index` os `component_id`s `track_title_<idx>`, `track_volume_<idx>`, `track_pan_<idx>`, `track_mute_<idx>`, `track_solo_<idx>` e `track_auto_<idx>` (mantendo `track_color_swatch_<idx>`), sincronizar `setToggleState(..., juce::dontSendNotification)` com cores `buttonOnColourId`/`textColourOnId` preservadas (`accentAmber`, `accentCyan`, `accentPurple`) mantendo `setClickingTogglesState(false)`, e mover `is_adjusting_slider_ = false` para antes de `model_.end_gesture()` em `onDragEnd`.
  - **Camada:** Studio (UI / MCP)
  - **Aceite:** `studio_ui_inspect` reporta todos os `component_id`s e `toggle_state` fiel; `studio_trigger_control` alterna `track_mute_0`/`track_solo_0` uma única vez; `studio_ui_set_control` altera `track_volume_0`/`track_pan_0` e aplica `toggle` idempotente em `track_mute_0`/`track_solo_0`/`track_auto_0` (validado em `--smoke-mcp` e `--smoke-pro-ui`).

## 4. Testes automatizados, documentação e verificação ao vivo

- [x] 4.1 Ampliar os smokes em `studio/Source/Main.cpp`:
  - `--smoke-transport-mixer`: validar mixagem ao vivo de faixas (`Mute`, `Solo`, precedência `Mute` vence `Solo`, `Volume`, `Pan`, reaplicação em `resume`/loop/hot-swap, ausência de rebuild em `M`/`S`) e rebuilds consecutivos durante reprodução de sample com `"stream": true`.
  - `--smoke-timeline`: validar preservação de `track_<idx>_mute`/`_solo` após `remove_track`, `duplicate_track` e `reorder_track`.
  - `--smoke-mcp`: validar `studio_ui_inspect`, `studio_trigger_control` e `studio_ui_set_control` (idempotente) sobre `track_mute_0`, `track_solo_0`, `track_auto_0`, `track_volume_0` e `track_pan_0`.
  - **Camada:** Studio (Testes / Harness)
  - **Aceite:** `./scripts/verify.ps1 -Layer studio` aprova 11/11 smokes em Release sem falhas.

- [x] 4.2 Atualizar documentação e exemplos de uso:
  - `docs/specs/mcp-server-guide.md`: documentar os novos `ComponentID`s determinísticos de cabeçalho de faixa (`track_title_<index>`, `track_volume_<index>`, `track_pan_<index>`, `track_mute_<index>`, `track_solo_<index>`, `track_auto_<index>`).
  - `docs/manual/chapters/05-timeline-e-waveforms.html` e `docs/manual/chapters/03-transporte-e-audicao.html`: documentar o comportamento em tempo real de `M`, `S`, `Volume` e `Pan` na audição, a precedência `Mute` vence `Solo`, o caráter exclusivo de audição de `M`/`S` (sem afetar o `.bank` publicado) e o hot-swap de prévia em projetos com streaming.
  - **Camada:** Documentação
  - **Aceite:** links e estrutura aprovados em `./scripts/verify.ps1 -Layer docs`.

- [x] 4.3 Executar gates finais e verificação ao vivo via MCP (em cópia temporária ou projeto `example_project/playground_project.json` sem salvar alterações):
  - Executar `./scripts/verify.ps1 -Layer studio,docs` e `openspec validate studio-live-track-mix-and-preview-swap --type change --strict`.
  - Verificar ao vivo via MCP (`studio_transport` `play`, `studio_ui_inspect`, `studio_trigger_control` em `track_mute_0`/`track_mute_1`, `studio_ui_set_control` em `track_volume_0`/`track_pan_0` e `studio_ui_screenshot` no `transport_ksystem_meter` + `Diagnostics`) que: (a) mutar Track 1 e Track 2 zera o meter imediatamente sem `Preview Build Error`, (b) ajustar Volume para `-80 dB` silencia e Pan para `-1.0`/`+1.0` desloca L/R no meter, e (c) o rebuild de prévia conclui com `Preview In Sync` em evento com `"stream": true`. Registrar evidências em `verification.md` da change e atualizar `docs/ai/memory.md`.
  - **Camada:** Verificação integrada (Studio + MCP + Docs)
  - **Aceite:** todos os gates com código de saída 0 e evidências MCP registradas.
