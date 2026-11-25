# Proposal

## Why

Na audição local do OpenAudio Studio, os controles de mixagem por faixa (`M` Mute, `S` Solo, `Volume` e `Pan`) em `TrackHeaderPanel`, `LayerStackView` e `InspectorPanel` (`TrackCard`) não produzem efeito no áudio em reprodução:

1. **Ausência de atualização em tempo real por faixa:** `AuthoringModel` grava `volume_db`/`pan` no JSON e `track_<idx>_mute`/`track_<idx>_solo` em `user_props`, disparando um rebuild completo do banco em `PreviewRebuilder`, sem nunca aplicar `oae_bus_set_volume`, `oae_bus_set_pan` ou `oae_bus_set_mute` nos buses das faixas via `oae_event_instance_get_track_bus`. Além disso, `oae_bankc` e `EventInstance` ignoram `user_props`, fazendo com que `Mute` e `Solo` nunca silenciem o áudio.
2. **Falha de substituição atômica do banco de prévia em projetos com streaming (`Preview Build Error`):** quando um evento usa samples com `"stream": true` (como `example_project/playground_project.json`), `Channel::init()` mantém um `std::ifstream` aberto sobre `build/preview.bank`. Como `PreviewRebuilder` invoca `oae_bankc` sobre o mesmo arquivo `build/preview.bank` antes de parar o transporte e descarregar o banco, `MoveFileExW` no Windows falha com `Atomic replacement failed: Acesso negado.`, bloqueando permanentemente até o fallback de recompilação da prévia.
3. **Lacuna de automação MCP na Timeline:** os controles de `TrackHeaderItem` (`M`, `S`, `A`, `Volume`, `Pan` e título) não expõem `component_id` determinístico e os botões `M`/`S`/`A` não sincronizam `toggle_state`, impedindo endereçamento semântico via `studio_trigger_control` / `studio_ui_set_control` e inspeção fiel via `studio_ui_inspect`.

## What Changes

- **Mixagem de faixas em tempo real no Studio (`Mute`, `Solo`, `Volume`, `Pan`):**
  - Expor `StudioEngine::get_instance_track_bus` (wrapper de `oae_event_instance_get_track_bus`) e getters de faixa (`get_track_count`, `get_track_volume`, `get_track_pan`, `get_track_output_track`) em `AuthoringModel`.
  - Conectar `AuditionTransport` como `AuthoringModel::Listener` para aplicar imediatamente (`0 ms`) `volume_db`, `pan` e `effective_mute` aos buses das faixas da instância ativa ao iniciar (`play`, `resume`, reinício de loop em `update_tick`) e a cada alteração no modelo (incluindo Undo/Redo), abrangendo Timeline (`TrackHeaderPanel`), `LayerStackView` (Olho, `S` e Opacidade) e Inspector (`TrackCard`).
  - Adotar a semântica do FMOD Studio e do `MixerDeckPanel`: `Mute` e `Solo` de faixa valem exclusivamente para audição no Studio (não são compilados no `.bank` publicado e não disparam rebuild de prévia) e aplicam precedência consistente com o Mixer (`Mute` vence `Solo`: sem nenhum `Solo` ativo, `effective_mute = muted`; com um ou mais `Solo` ativos, `effective_mute = !keep_audible || muted`, respeitando ancestralidade `output_track`).
  - Preservar o alinhamento das chaves `track_<idx>_mute` / `track_<idx>_solo` em `user_props` ao remover, duplicar ou reordenar faixas em `AuthoringModel`.
- **Hot-swap seguro do banco de prévia com streaming:**
  - Alterar `PreviewRebuilder` para compilar em dois slots alternados (`build/preview_a.bank` e `build/preview_b.bank`), evitando colisão com o arquivo atualmente aberto por `Channel` em streaming no Windows.
  - Em `apply_compiled_bank` e `unload_preview_bank`, parar transporte/snapshots, descarregar o banco anterior e executar `engine_.update()` para destruir os canais finalizados e fechar seus handles de arquivo antes de trocar de slot e limpar o arquivo anterior.
  - Adiar o disparo do rebuild em `PreviewRebuilder::timerCallback()` enquanto `AuthoringModel::is_in_gesture()` estiver ativo (durante arraste de slider) e ignorar rebuild quando `content_revision_` não mudou (alterações somente de audição como `M`/`S`).
  - Atualizar `.gitignore` para cobrir `example_project/build/preview*.bank*`.
- **Identificadores semânticos e `toggle_state` para MCP em `TrackHeaderPanel`:**
  - Atribuir `component_id` determinísticos em `TrackHeaderItem` (`track_title_<idx>`, `track_volume_<idx>`, `track_pan_<idx>`, `track_mute_<idx>`, `track_solo_<idx>`, `track_auto_<idx>`, além do `track_color_swatch_<idx>` já existente), atualizando-os em `set_track_index()`.
  - Sincronizar `setToggleState(..., juce::dontSendNotification)` e `buttonOnColourId`/`textColourOnId` em `muteButton`, `soloButton` e `autoButton` (mantendo `setClickingTogglesState(false)` para compatibilidade com `studio_trigger_control` sem duplo toggle e idempotência em `studio_ui_set_control`).
  - Corrigir a ordem em `onDragEnd` dos sliders de `TrackHeaderItem` para liberar `is_adjusting_slider_` antes de `model_.end_gesture()`.

## Capabilities

### New Capabilities

### Modified Capabilities

- `studio-shell` — mixagem de faixas em tempo real na audição do Studio (`Mute`, `Solo`, `Volume`, `Pan`) com precedência consistente e hot-swap seguro do banco de prévia em projetos com streaming.
- `mcp-automation` — `component_id` determinísticos e `toggle_state` sincronizado para os controles de cabeçalho de faixa na Timeline (`TrackHeaderPanel`).

## Impact

- **Studio (`studio/Source/`):** `AuthoringModel`, `StudioEngine`, `AuditionTransport`, `PreviewRebuilder`, `TrackHeaderPanel` e smokes (`--smoke-transport-mixer`, `--smoke-mcp`, `--smoke-pro-ui`).
- **Infraestrutura do repositório:** `.gitignore` para os slots rotativos `preview_a.bank` / `preview_b.bank`.
- **Documentação:** `docs/specs/mcp-server-guide.md`, `docs/manual/chapters/05-timeline-e-waveforms.html` e `docs/manual/chapters/03-transporte-e-audicao.html`.
- **Exclusões:** alterações na ABI C (`native/include/oae_audio.h`), no DSP/runtime nativo (`native/src/`), no compilador de bancos (`oae_bankc`), no formato binário de banco v1 (`project-v1` / `EVTS`), no bridge Unity e em mensagens remotas de mixagem por faixa via Live-Linking (o transporte remoto segue inalterado).
