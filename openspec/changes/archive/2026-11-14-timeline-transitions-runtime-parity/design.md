# Design

## Context

Conforme mapeado em [proposal.md](proposal.md), o schema de projeto, o `AuthoringModel` e o compilador `oae_bankc` já serializam marcadores de destino (`LogicItemKind::Marker`), marcadores pontuais de transição (`LogicItemKind::TransitionMarker`), regiões de transição (`LogicItemKind::TransitionRegion`), parâmetros `quantization`/`crossfade_ms` e a propriedade `polyphony` em `Scatterer Instrument`. Contudo, o runtime em `EventInstance` apresenta lacunas de paridade que impedem o funcionamento completo dessas regras no motor C++.

## Goals / Non-Goals

**Goals:**
- Garantir que `EventInstance` resolva destinos de transição tanto para `LogicItemKind::Marker` (padrão canônico do compilador/Studio) quanto para `LogicItemKind::TransitionMarker` sem destino (compatibilidade legada).
- Suportar avaliação completa em runtime de `LogicItemKind::TransitionMarker` pontual (imediato e quantizado por `Beat`/`Bar`, sujeito a `evaluate_condition`) incluindo restrição de `pre_scheduled_until_ms_` em `EventInstance::start()`.
- Implementar crossfade suave (`crossfade_ms > 0`) durante saltos em `sync_timeline_jump()` sem quebrar saltos secos (`crossfade_ms <= 0`) nem envelopes `AHDSR`.
- Implementar descarte de grãos *Sound Stealing (`Oldest`)* em `EventInstance::spawn_scatterer_grain()` quando a polifonia ativa atingir `inst.polyphony`.

**Non-Goals:**
- Alterar a ABI C (`oae_audio.h`), o formato binário `.bank` ou a camada de autoria do Studio.
- Implementar outros modos de stealing por grão (`Quietest`, `Furthest`) ou novos algoritmos no `Scatterer`.

## Decisions

1. **Auxiliar `resolve_marker_destination_ms(const std::string& name, double& out_pos_ms)`**:
   - Centraliza a busca em `model->logic_items`, aceitando `target.kind == LogicItemKind::Marker || (target.kind == LogicItemKind::TransitionMarker && target.destination.empty())`.
   - **Alternativa considerada**: Alterar o compilador `oae_bankc` para emitir `TransitionMarker` para marcadores simples; descartada pois quebraria callbacks `OAE_EVENT_CALLBACK_MARKER` e semântica FMOD canônica.

2. **Estado unificado para transições quantizadas (`pending_transition_quant_`, `pending_transition_xfade_ms_`, `pending_transition_arm_ms_`)**:
   - Em vez de procurar novamente apenas por `TransitionRegion` ao cruzar batida/compasso, `update()` armazena a quantização, o `crossfade_ms` e o instante em que a transição foi armada (`pending_transition_arm_ms_`) quando uma `TransitionMarker` ou `TransitionRegion` entra em vigor.
   - Na malha de batidas (`OAE_EVENT_CALLBACK_BEAT`), o disparo ocorre somente quando `next_beat_index_ > 0` (evitando disparo prematuro no beat `0` idêntico a `Immediate`), quando `props.position_ms + 1.0e-3 >= pending_transition_arm_ms_` e a fronteira rítmica (`Beat` ou `Bar` com `props.beat == 0`) é atingida.
   - `sync_timeline_jump()` passa a aceitar `double crossfade_ms = 0.0` e redefine `pre_scheduled_until_ms_ = timeline_position_ms_` para garantir que instrumentos futuros após o salto voltem a ser agendados corretamente em `update()`.
   - Um salto imediato no mesmo tick marca `jumped_by_transition = true` para impedir que uma `LoopRegion` encerrando no mesmo instante sobrescreva o destino do salto.

3. **Crossfade em `sync_timeline_jump(double new_pos_ms, double crossfade_ms)`**:
   - Quando `crossfade_ms > 0.0`:
     - Vozes síncronas ativas (`!v.async`) em `active_voices_` recebem `oae_channel_stop_with_fade(v.channel, fade_ms)` e são removidas de `active_voices_`, mantendo seus handles em `playing_channels_` para que concluam o fade-out na engine de áudio e sejam recolhidas automaticamente por `update()` ao terminarem.
     - Vozes síncronas recém-disparadas na posição de destino recebem `oae_channel_fade_to(ch, 0.0f, 0, OAE_FADE_LINEAR)` seguido de `oae_channel_fade_to(ch, 1.0f, fade_ms, OAE_FADE_LINEAR)`. O uso do fader do `StripNode` preserva o volume/automação calculados via `oae_channel_set_volume`.
   - **Alternativa considerada**: Pausar e reutilizar canais pré-existentes fazendo seek sem crossfade quando `existing != 0`; mantida somente para o caminho `crossfade_ms <= 0.0`, já que o fade-out/fade-in simultâneo exige dois canais coexistindo durante `crossfade_ms` mesmo se o salto ocorrer dentro do mesmo instrumento longo.

4. **Sound Stealing (`Oldest`) no Scatterer (`steal_oldest_scatterer_grain`)**:
   - Como `active_voices_` e `active_child_events_` inserem novos disparos via `push_back` em ordem cronológica estrita, o primeiro elemento ainda tocando correspondente ao par `(track_idx, inst_idx)` é o grão mais antigo daquele spawner.
   - Em `spawn_scatterer_grain()`, enquanto `inst.polyphony > 0 && count_active_scatterer_grains(state.track_idx, state.inst_idx) >= inst.polyphony`, o motor interrompe o grão mais antigo (`oae_channel_stop` + remoção de `active_voices_` e `playing_channels_`, ou `ce.instance->stop(OAE_STOP_IMMEDIATE)` + remoção de `active_child_events_`) antes de sortear e disparar o novo grão.
   - Se por algum estado inconsistente nenhum grão removível for encontrado em uma iteração, o loop é interrompido para evitar laço infinito.

## Risks / Trade-offs

- [Consumo temporário dobrado de vozes durante crossfade] → Vozes síncronas anteriores seguem em `playing_channels_` somente até completar `crossfade_ms` (~poucas dezenas/centenas de milissegundos), sendo podadas no primeiro `update()` após o término.
- [Retrocompatibilidade de testes legados] → Fixtures antigas que usavam `LogicItemKind::TransitionMarker` sem `destination` como marcador alvo continuam resolvidas normalmente em `resolve_marker_destination_ms`.
