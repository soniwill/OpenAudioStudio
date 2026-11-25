# Spec Delta

## ADDED Requirements

### Requirement: Identificadores semânticos e estado de alternância dos controles de cabeçalho de faixa
O `TrackHeaderPanel` (`TrackHeaderItem`) SHALL expor identificadores semânticos determinísticos (`ComponentID`) para todos os controles interativos de cada faixa da Timeline e SHALL manter o `toggle_state` dos botões `M`, `S` e `A` sincronizado com o estado real da faixa para inspeção e automação via MCP.

#### Scenario: Inspeção de controles de faixa por ComponentID e toggle_state via MCP
- **WHEN** o cliente MCP invoca `studio_ui_inspect` sobre `timeline_view` em um evento com `N` faixas (`0..N-1`)
- **THEN** cada cabeçalho de faixa expõe os componentes com `component_id` `track_color_swatch_<i>`, `track_title_<i>`, `track_volume_<i>`, `track_pan_<i>`, `track_mute_<i>`, `track_solo_<i>` e `track_auto_<i>`
- **AND** `track_mute_<i>`, `track_solo_<i>` e `track_auto_<i>` reportam `toggle_state` fiel ao estado atual (`true` quando ativo, `false` quando inativo), mantendo as cores semânticas ativas (`accentAmber` para `M`, `accentCyan` para `S` e `accentPurple` para `A`)
- **AND** ao reordenar ou remover faixas, `set_track_index` atualiza todos os `component_id`s para refletir os novos índices

#### Scenario: Acionamento determinístico e idempotente via studio_trigger_control e studio_ui_set_control
- **WHEN** o cliente MCP aciona `studio_trigger_control` com `track_mute_0` ou `track_solo_0`
- **THEN** o botão alterna exatamente uma vez (sem duplo toggle), atualizando `AuthoringModel`, `toggle_state` e a audição em tempo real
- **AND** quando o cliente MCP invoca `studio_ui_set_control` com `{"component_id": "track_mute_0", "toggle": true}` (ou `false`), a operação é idempotente e sincroniza estado visual, modelo e áudio sem dessincronia de cores
- **AND** quando o cliente MCP invoca `studio_ui_set_control` com `{"component_id": "track_volume_0", "value": -12.0}` ou `{"component_id": "track_pan_0", "value": -1.0}`, o slider correspondente e o áudio em reprodução refletem o novo valor imediatamente
