# Design

## Context

A investigação ao vivo via MCP sobre `event:/playground/adaptive_music` (`example_project/playground_project.json`) e a inspeção do código-fonte revelaram três lacunas interligadas na camada do Studio:

1. **`EventInstance` já suporta mixagem por faixa no motor nativo**, mas o Studio nunca a consome em tempo real:
   - `EventInstance::create_track_buses()` (`native/src/core/event_instance.cpp:438-518`) cria um `oae_bus` dedicado (`trk_<t>`) para cada faixa dentro de `EventInstance::start()` (`:1685`), aplicando `volume_db`, `pan` e efeitos compilados.
   - `StripNode::apply_fader()` (`native/src/core/strip.cpp:233-263`) aplica ganho, pan equal-power e silenciamento (`strip.muted()`) nesse bus.
   - A API C já expõe `oae_event_instance_get_track_bus(instance, track_index, &out_bus)` (`native/include/oae_audio.h:355`), `oae_bus_set_volume`, `oae_bus_set_pan` e `oae_bus_set_mute`.
   - Contudo, nada em `studio/Source/` chama `oae_event_instance_get_track_bus`. `AuditionTransport` (`studio/Source/Transport/AuditionTransport.cpp`) não observa `AuthoringModel`, e todas as alterações de faixa dependiam exclusivamente de `PreviewRebuilder` (que recompila o banco inteiro após 600 ms e ignora `user_props` de `Mute`/`Solo`).
2. **Bloqueio de arquivo no Windows em `PreviewRebuilder` com `"stream": true`**:
   - Samples em streaming (`sample_music_explore`, `sample_music_combat`, `sample_music_tension`) abrem `sliceSource_.file.open(data_->streamPath, std::ios::binary)` em `Channel::init()` (`native/src/core/channel.cpp:58-68`), mantendo handle aberto sobre `build/preview.bank`.
   - `PreviewRebuilder::timerCallback()` (`studio/Source/Transport/PreviewRebuilder.cpp:72-73`) invoca `oae_bankc` sobre o mesmo caminho `build/preview.bank` enquanto o banco está carregado e tocando. No Windows, `MoveFileExW` em `native/tools/bankc/publication.cpp:165-166` falha com `ERROR_ACCESS_DENIED` (`Atomic replacement failed: Acesso negado.`).
   - Adicionalmente, `Channel` só é destruído em `Runtime::update_voices()` (`native/src/api/runtime.cpp:80-82`), executado durante `oae_system_update()` (`engine_.update()`), que `PreviewRebuilder::apply_compiled_bank()` hoje não invoca após `stop(true)` / `unload_bank()`.
3. **`TrackHeaderPanel` sem IDs MCP e com `toggle_state` dessincronizado**:
   - Em `studio/Source/UI/TrackHeaderPanel.cpp`, apenas `colorSwatchButton_` define `component_id`. `muteButton`, `soloButton` e `autoButton` usam `setClickingTogglesState(false)` e nunca chamam `setToggleState(...)`, deixando `studio_ui_inspect` sempre com `toggle_state: false` e fazendo `studio_ui_set_control({"toggle": true})` pintar o botão com o estilo ciano padrão (`OaeDarkLookAndFeel::buttonOnColourId`) fora de sincronia com o modelo.
   - Em `onDragEnd` (`TrackHeaderPanel.cpp:51-54, 71-74`), `model_.end_gesture()` roda antes de `is_adjusting_slider_ = false`, fazendo `TimelinePanel::on_project_changed()` suprimir o refresh final.

## Goals / Non-Goals

### Goals
- Aplicar `Mute`, `Solo`, `Volume` e `Pan` de faixas em tempo real (`0 ms`) nos buses de faixa da instância em audição local, sincronizados entre Timeline (`TrackHeaderPanel`), `LayerStackView`, `InspectorPanel` (`TrackCard`) e Undo/Redo.
- Manter `Mute` e `Solo` estritamente como estado de audição do Studio (alinhado ao FMOD Studio), sem recompilar banco e sem contaminar `.bank` publicados.
- Eliminar `Preview Build Error: Atomic replacement failed: Acesso negado.` em projetos com streaming no Windows sem interromper o áudio durante a compilação em background.
- Expor `component_id` determinísticos e `toggle_state` fiel em `TrackHeaderItem` para inspeção e automação MCP confiáveis.

### Non-Goals
- Não alterar a ABI C, o formato binário de banco v1 (`oae::Track` / `EVTS`), o schema `project-v1` nem o compilador `oae_bankc`.
- Não enviar comandos de mixagem por faixa via `LiveBridgeServer` (escopo separado de Live-Linking).
- Não alterar `OaeDarkLookAndFeel` globalmente para outros botões fora do escopo necessário.

## Decisions

### 1. Separação entre mutações compiláveis e estado de audição em `AuthoringModel`
- Adicionar `AuthoringModel::modify_audition(const std::function<void(nlohmann::json&)>& mutator)` (ou parâmetro booleano `bool affects_compiled_bank = true` em `modify(...)`):
  - Registra snapshot de Undo/Redo normalmente quando `!in_gesture_` e chama `notify_project_changed()` para que todas as views (`TrackHeaderPanel`, `LayerStackView`, `InspectorPanel` e `AuditionTransport`) atualizem imediatamente.
  - Quando `affects_compiled_bank == false` (usado por `set_track_muted` e `set_track_solo`), **não incrementa `content_revision_`**.
- Adicionar getters de leitura de faixa em `AuthoringModel`:
  - `size_t get_track_count(const std::string& event_path) const`
  - `double get_track_volume(const std::string& event_path, size_t track_index) const`
  - `double get_track_pan(const std::string& event_path, size_t track_index) const`
  - `int get_track_output_track(const std::string& event_path, size_t track_index) const` (lendo `output_track` se presente, padrão `-1`).
- Em `remove_track`, `duplicate_track` e `reorder_track` (`AuthoringModel.cpp:1364-1426`), adicionar helper local para remapear as chaves `track_<idx>_mute` e `track_<idx>_solo` (e demais `track_<idx>_*` de metadados de faixa quando presentes, preservando comportamento existente de nome/cor) em `ev["user_props"]`, evitando deslocamento de estado ao reorganizar faixas.
- *Tradeoff:* ao desfazer (`undo`/`redo`) um snapshot via `apply_snapshot()`, `content_revision_` continua incrementando (seguro com o ping-pong de prévia), enquanto cliques diretos em `M`/`S` nunca disparam `PreviewRebuilder`.

### 2. Ponte de mixagem por faixa em tempo real em `StudioEngine` e `AuditionTransport`
- **Wrapper em `StudioEngine`:** adicionar `oae_result get_instance_track_bus(oae_event_instance instance, uint32_t track_index, oae_bus* out_bus)` delegando para `oae_event_instance_get_track_bus(instance, track_index, out_bus)`.
- **Observação do modelo em `AuditionTransport`:**
  - `AuditionTransport` passa a herdar também de `AuthoringModel::Listener`, registrando-se via `model_.add_listener(this)` no construtor e removendo-se via `model_.remove_listener(this)` no destrutor.
  - Implementar `void sync_active_track_mix()` (público ou privado chamado por `on_project_changed()` e após partida da instância):
    - Se `state_ != TransportState::Playing || active_instance_ == 0 || event_path_.empty()`, retorna imediatamente.
    - Consulta `const size_t n = model_.get_track_count(event_path_)`.
    - Calcula o conjunto de faixas com `Solo` ativo (`any_solo`). Para cada faixa `t`:
      - Se `!any_solo`: `effective_mute = muted[t]`.
      - Se `any_solo`: `keep_audible = solo[t]` ou `t` é ancestral/descendente de alguma faixa solada na cadeia `output_track` (mesma semântica hierárquica de `MixerDeckPanel::update_hierarchical_solo`), e `effective_mute = !keep_audible || muted[t]` (`Mute` vence `Solo`).
    - Para cada faixa `t`, chama `engine_.get_instance_track_bus(active_instance_, static_cast<uint32_t>(t), &trk_bus)`:
      - Se retornar `OAE_OK && trk_bus != 0`: aplica `engine_.set_bus_volume_db(trk_bus, static_cast<float>(vol_db))`, `engine_.set_bus_pan(trk_bus, static_cast<float>(pan))` e `engine_.set_bus_mute(trk_bus, effective_mute)`.
- **Pontos obrigatórios de invocação (armadilha de ciclo de vida de `track_buses_`):**
  - Como `EventInstance::create_track_buses()` executa dentro de `EventInstance::start()` (`event_instance.cpp:1685`), `sync_active_track_mix()` MUST ser chamado **imediatamente após** `oae_event_instance_start(active_instance_)` em todos os três pontos:
    1. `AuditionTransport::play()` (`AuditionTransport.cpp:138`)
    2. `AuditionTransport::resume()` (`AuditionTransport.cpp:186`)
    3. `AuditionTransport::update_tick()` no reinício por loop (`AuditionTransport.cpp:387`) — crítico porque `start()` recria `track_buses_` com os valores compilados do banco.
  - E também em `AuditionTransport::on_project_changed()`, cobrindo edições ao vivo e Undo/Redo.

### 3. Hot-swap seguro em dois slots alternados (`preview_a.bank` / `preview_b.bank`) em `PreviewRebuilder`
- **Alternância ping-pong:**
  - `PreviewRebuilder` mantém `int active_preview_slot_{ -1 }` e `fs::path active_preview_bank_path_`.
  - A cada compilação (`timerCallback()` e `rebuild_synchronously()`), escolhe o slot inativo:
    `const int next_slot = (active_preview_slot_ == 0) ? 1 : 0;`
    `const fs::path preview_bank = doc_.get_project_dir() / "build" / (next_slot == 0 ? "preview_a.bank" : "preview_b.bank");`
  - Usar exatamente dois nomes fixos limita o número de arquivos `.bankc-lock-<sha256>` gerados por `oae_bankc` (`publication.cpp:302`) a no máximo dois em `build/` (evitando acúmulo infinito de arquivos de lock que ocorreria com `preview_<rev>.bank`).
- **Filtro de rebuild e adiamento durante gesto:**
  - Em `PreviewRebuilder::on_project_changed()`: se `loaded_preview_bank_ != 0 && model_.get_content_revision() == last_rebuilt_revision_`, retornar sem agendar rebuild (evitando compilar em alterações de `M`/`S`).
  - Em `PreviewRebuilder::timerCallback()`: se `model_.is_in_gesture()`, reagendar `startTimer(600)` e retornar, garantindo arraste contínuo sem cortes enquanto `AuditionTransport` atualiza volume/pan ao vivo.
- **Liberação determinística de handles de streaming em `apply_compiled_bank` e `unload_preview_bank`:**
  - Em `apply_compiled_bank(const fs::path& bank_path, int slot)`:
    1. Captura `AuditionSnapshotState snap` via `transport_.capture_state(snap)`.
    2. Se `snap.was_playing`, chama `transport_.stop(true)`; também chama `transport_.stop_all_snapshots()` para garantir que nenhuma instância de snapshot segure referências ao banco antigo.
    3. Se `loaded_preview_bank_ != 0`, chama `engine_.unload_bank(loaded_preview_bank_); loaded_preview_bank_ = 0;`.
    4. Chama `engine_.update()` para executar `Runtime::update_voices()` (`native/src/api/runtime.cpp:80-82`), destruindo canais finalizados e fechando `Channel::sliceSource_.file` (`std::ifstream`) imediatamente.
    5. Carrega `bank_path` via `engine_.load_bank_file(...)`, atualiza `active_preview_slot_ = slot` e `active_preview_bank_path_ = bank_path`, e chama `transport_.restore_state(snap)` (que reinicia `play()` e reaplica `sync_active_track_mix()`).
    6. Remove em modo best-effort (`std::error_code ec`) o arquivo `.bank` e `.bank.catalog.json` do slot anterior (se diferente de `bank_path`).
  - Se `!doc_.is_loaded()` quando o callback assíncrono terminar, remove em best-effort o `preview_bank` (e `.catalog.json`) recém-compilado descartado.
  - Em `unload_preview_bank()` / `~PreviewRebuilder()`: para o timer, descarrega `loaded_preview_bank_`, executa `engine_.update()` e limpa em best-effort `active_preview_bank_path_` (+ `.catalog.json`), resetando `active_preview_slot_ = -1`.
- **`.gitignore`:** atualizar `/example_project/build/preview.bank*` para `/example_project/build/preview*.bank*`.

### 4. `ComponentID`s determinísticos e `toggle_state` em `TrackHeaderItem`
- Em `TrackHeaderPanel.cpp`:
  - Criar helper `update_component_ids()` chamado no construtor e em `set_track_index(size_t)`:
    - `colorSwatchButton_.setComponentID("track_color_swatch_" + idx)`
    - `titleLabel.setComponentID("track_title_" + idx)`
    - `volumeSlider.setComponentID("track_volume_" + idx)`
    - `panSlider.setComponentID("track_pan_" + idx)`
    - `muteButton.setComponentID("track_mute_" + idx)`
    - `soloButton.setComponentID("track_solo_" + idx)`
    - `autoButton.setComponentID("track_auto_" + idx)`
  - Manter `setClickingTogglesState(false)` em `muteButton`, `soloButton` e `autoButton` (evitando duplo toggle em `McpServer::tool_trigger_control`, `McpServer.cpp:454-462`), mas sincronizar `setToggleState(state, juce::dontSendNotification)` em `update_from_model()` e no `autoButton.onClick`.
  - Configurar explicitamente as cores de ambos os estados (`buttonColourId`/`textColourOffId` e `buttonOnColourId`/`textColourOnId`) em `muteButton` (`accentAmber` / `bgDark`), `soloButton` (`accentCyan` / `bgDark`) e `autoButton` (`accentPurple` / `textPrimary`) para preservar a identidade visual sob `OaeDarkLookAndFeel` (que consulta `buttonOnColourId` e `textColourOnId` quando `getToggleState() == true`).
  - *Idempotência verificada com `McpServer::tool_ui_set_control` (`McpServer.cpp:1051-1056`):* como `JUCE::Button::setToggleState(target, sendNotificationSync)` só dispara `onClick` quando `target != lastToggleState`, e `lastToggleState` agora sempre espelha o modelo, `toggle: true` em botão já ativo é no-op e em botão inativo dispara `onClick` uma única vez, mantendo UI, `toggle_state` e modelo idênticos.
  - Em `volumeSlider.onDragEnd` e `panSlider.onDragEnd`, definir `is_adjusting_slider_ = false;` **antes** de `model_.end_gesture();` para que o `on_project_changed()` final não seja ignorado por `TimelinePanel`.

## Risks / Trade-offs

- **Reconstrução em loop (`update_tick`) ou troca de slot (`restore_state`):** se `sync_active_track_mix()` for esquecido em qualquer um dos caminhos de partida (`play`, `resume`, `update_tick`), o áudio voltaria momentaneamente aos valores compilados no banco (desmutando faixas mutadas). Cobrir explicitamente os três caminhos em `--smoke-transport-mixer`.
- **Contenção do audio thread no Windows ao fechar `std::ifstream`:** em áudio real com dispositivo ativo, uma chamada de `engine_.update()` ocorre na Message Thread enquanto o callback de áudio do miniaudio roda em outra thread; como o `PreviewRebuilder` compila no slot oposto antes de trocar, mesmo que a remoção best-effort do slot antigo falhe pontualmente por atraso de um frame, o slot novo já foi compilado e carregado sem erro, e o slot antigo só será reutilizado após o próximo ciclo de rebuild (>600 ms depois, quando já está fechado).
- **Ordem de destruição entre `AuditionTransport` e `AuthoringModel`:** como `MainComponent` declara `authoringModel_` antes de `transport_` (e o mesmo nos smokes), `transport_` é destruído antes de `authoringModel_` e remove-se com segurança via `model_.remove_listener(this)` no destrutor.
- **Compatibilidade com smokes existentes:** nenhum smoke ou script referencia literalmente `build/preview.bank`, mas `--smoke-transport-mixer` e `--smoke-mcp` devem ser executados e estendidos para cobrir streaming, hot-swap repetido, mixagem ao vivo e novos `component_id`s.
