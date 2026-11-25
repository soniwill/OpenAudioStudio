# Verificação — `studio-live-track-mix-and-preview-swap`

Data: 2026-11-19

## 1. Gates automatizados

- `openspec validate studio-live-track-mix-and-preview-swap --type change --strict` — aprovado (`Change 'studio-live-track-mix-and-preview-swap' is valid`, exit 0).
- `./scripts/verify.ps1 -Layer studio` (Release) — build completo sem avisos novos e **11/11 smoke tests aprovados** (`RESULT: OK`, exit 0):
  - `--smoke-headless`: PASSED
  - `--smoke-project`: PASSED
  - `--smoke-authoring`: PASSED
  - `--smoke-timeline`: PASSED (valida `set_track_muted`/`set_track_solo` sem incrementar `content_revision_`, Undo/Redo e remapeamento de `track_<idx>_mute`/`_solo` em `duplicate_track`, `reorder_track` e `remove_track`)
  - `--smoke-transport-mixer`: PASSED (valida `StudioEngine::get_instance_track_bus`, mixagem ao vivo `Mute`/`Solo`/`Volume`/`Pan`, precedência `Mute` vence `Solo` com hierarquia `output_track`, Undo/Redo, reaplicação em `resume()` e reinício de loop em `update_tick()`, adiamento de `PreviewRebuilder::timerCallback()` durante gesto e 3 hot-swaps consecutivos em slots alternados sobre sample com `"stream": true` após renderizar frames)
  - `--smoke-end-to-end`: PASSED
  - `--smoke-device`: PASSED
  - `--smoke-pro-ui`: PASSED (valida `ComponentID`s de `TrackHeaderItem`, `toggle_state`, cores semânticas `buttonOnColourId`/`textColourOnId`, atualização em `set_track_index` após reorder e ordem `is_adjusting_slider_ = false` antes de `model_.end_gesture()` em `onDragEnd`)
  - `--smoke-mcp`: PASSED (valida `studio_ui_inspect` sobre `timeline_view`, single-toggle via `studio_trigger_control` em `track_mute_0`/`track_solo_0`, idempotência de `studio_ui_set_control` em `track_mute_0`/`track_auto_0` e atualização imediata do bus de faixa ao ajustar `track_volume_0`/`track_pan_0`)
  - `--smoke-vu`: PASSED
  - `--smoke-live`: PASSED
- `./scripts/verify.ps1 -Layer docs` — aprovado (`openspec validate --all --strict`: 12/12 itens válidos, `docs/ai/memory.md` dentro do limite de 100 linhas, links relativos validados, `RESULT: OK`, exit 0).

## 2. Verificação ao vivo via MCP (`event:/playground/adaptive_music`)

Executada contra `build/Studio_Bundle_Release/OpenAudio Studio.exe` com `build/Studio_Bundle_Release/example_project/playground_project.json` (sem salvar alterações em `example_project/playground_project.json`), via `scripts/mcp_call.js` e leitura de recursos JSON-RPC na porta `127.0.0.1:7451`.

### Evidências registradas em `evidence/`

| Etapa | Ação MCP | Resultado observado | Arquivos de evidência |
| :--- | :--- | :--- | :--- |
| Seleção e inspeção inicial | `studio_select_item` (`event:/playground/adaptive_music`), `studio_ui_inspect` (`timeline_view`, `status`) | Todos os `track_*_0` e `track_*_1` expostos (`toggle_state: false` para `M`/`S`/`A`); status `Preview In Sync (Rev 0)` | 01_select_event.json (evidência local), 02_status_init.json (evidência local), 02_inspect_timeline_init.json (evidência local) |
| Play baseline | `studio_transport` (`play`), `studio_ui_screenshot`, `oae://audio/meters` | L/R ativos em `transport_ksystem_meter` (`bus:/music` peak `-47.04 dB`, rms `-55.71 dB`), status `Preview In Sync (Rev 5)` | 04_meter_baseline.png (evidência local), 04_full_baseline.png (evidência local), 04_meters_baseline.json (evidência local) |
| Mute Track 1 & 2 + Idempotência | `studio_trigger_control` (`track_mute_0`, `track_mute_1`) + `studio_ui_set_control` (`track_mute_0`, `toggle: true`) | `toggle_state: true` nos dois botões `M` (cor `accentAmber`), medidor zerado (`-80.0 dB` em `bus:/` e `bus:/music`), sem rebuild (`Preview In Sync (Rev 5)`) e sem erros no log | 05_trigger_mute0.json (evidência local), 05_trigger_mute1.json (evidência local), 05_set_mute0_idem.json (evidência local), 05_meter_muted.png (evidência local), 05_full_muted.png (evidência local), 05_meters_muted.json (evidência local), 05_diag_muted.json (evidência local) |
| Volume `-80 dB` | `studio_ui_set_control` (`track_volume_0`, `-80.0`) | Medidor zerado (`-80.0 dB` em `bus:/` e `bus:/music`) e `07_meter_vol_m80.png` sem sinal | 07_set_vol_m80.json (evidência local), 07_meter_vol_m80.png (evidência local), 07_meters_vol_m80.json (evidência local) |
| Pan `-1.0` + Hot-swap streaming #1 | `studio_ui_set_control` (`track_pan_0`, `-1.0`), aguarda >1,8 s | Barra superior (L) ativa e barra inferior (R) vazia em `08_meter_pan_left.png`; `Preview In Sync (Rev 8)`; `[Preview] Live preview hot swapped.` sem `Preview Build Error` | 08_set_pan_left.json (evidência local), 08_meter_pan_left.png (evidência local), 08_full_pan_left.png (evidência local), 08_status_pan_left.json (evidência local), 08_meters_pan_left.json (evidência local), 08_diag_pan_left.json (evidência local) |
| Pan `+1.0` + Hot-swap streaming #2 | `studio_ui_set_control` (`track_pan_0`, `1.0`), aguarda >1,8 s | Barra inferior (R) ativa e barra superior (L) vazia em `09_meter_pan_right.png`; `Preview In Sync (Rev 9)`; `[Preview] Live preview hot swapped.` sem `Preview Build Error` nem `Atomic replacement failed` | 09_set_pan_right.json (evidência local), 09_meter_pan_right.png (evidência local), 09_full_pan_right.png (evidência local), 09_status_pan_right.json (evidência local), 09_diag_pan_right.json (evidência local) |
