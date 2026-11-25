# Proposal

## Why

Na auditoria comparativa contra o benchmark FMOD Studio / Wwise (`LOG-03`, `LOG-04`, `MOD-03`), identificou-se que quatro comportamentos declarados no modelo e na autoria falham ou operam parcialmente em runtime no motor C++ (`EventInstance`):
1. O compilador `oae_bankc` e o `AuthoringModel` gravam marcadores de destino como `LogicItemKind::Marker`, porém `EventInstance::update()` procura exclusivamente `LogicItemKind::TransitionMarker` ao resolver `item.destination`, impedindo saltos para marcadores em bancos compilados.
2. Itens pontuais `LogicItemKind::TransitionMarker` (saltos imediatos ou quantizados "Transition To") não são avaliados em `EventInstance::update()` nem limitam `pre_scheduled_until_ms_` em `EventInstance::start()`.
3. O campo `crossfade_ms` em `TransitionMarker` e `TransitionRegion` é ignorado durante `sync_timeline_jump()`, produzindo cortes secos em vez de sobreposição gradual entre vozes síncronas.
4. Em `EventInstance::spawn_scatterer_grain()`, ao atingir `inst.polyphony`, o motor descarta silenciosamente o novo grão em vez de aplicar *Sound Stealing (Oldest)* interrompendo o grão ativo mais antigo conforme o requisito `MOD-03`.

## What Changes

- **Resolução de marcadores de destino (`timeline-logic`)**: Atualizar a resolução de destinos em `EventInstance` para localizar `LogicItemKind::Marker` (padrão canônico do compilador e Studio), mantendo retrocompatibilidade com `LogicItemKind::TransitionMarker` cujo `destination` esteja vazio.
- **Avaliação pontual de `TransitionMarker` e quantização (`timeline-logic`)**:
  - Limitar `pre_scheduled_until_ms_` no `start()` ao encontrar `TransitionMarker` com destino configurado à frente do cursor inicial.
  - Avaliar `TransitionMarker` cruzados no intervalo `[t0, t1]` em `EventInstance::update()`, respeitando `eval_condition(item.condition)` e quantização (`Immediate`, `Beat`, `Bar`).
  - Generalizar transições quantizadas pendentes (`pending_transition_quant_`, `pending_transition_xfade_ms_`) para `TransitionMarker` e `TransitionRegion`.
- **Crossfade em transições síncronas (`timeline-logic`)**: Aplicar `crossfade_ms > 0` em `sync_timeline_jump()` fazendo fade-out (`oae_channel_stop_with_fade`) nas vozes síncronas anteriores e fade-in (`oae_channel_fade_to`) nas novas vozes síncronas disparadas na posição de destino.
- **Sound Stealing Oldest no Scatterer (`instruments`)**: Em `EventInstance::spawn_scatterer_grain()`, quando a contagem de grãos ativos de `(track_idx, inst_idx)` atingir `inst.polyphony`, encerrar o grão ativo mais antigo (voz ou instância de evento filho) antes de disparar o próximo grão.
- **Exclusões (fora de escopo desta change)**: Magnet Regions, alterações de ABI C (`oae_audio.h`), alterações no formato binário `.bank`, novas propriedades no `AuthoringModel` ou UI do Studio (tratadas em etapas posteriores).

## Capabilities

### New Capabilities

<!-- Nenhuma nova capacidade introduzida nesta change. -->

### Modified Capabilities

- `timeline-logic`: Adicionar requisitos de runtime cobrindo resolução de `LogicItemKind::Marker`, avaliação imediata/quantizada de `TransitionMarker` e `TransitionRegion` e execução de `crossfade_ms` em saltos de transição.
- `instruments`: Adicionar requisito de runtime para *Sound Stealing (`Oldest`)* quando a polifonia ativa de um `Scatterer Instrument` atinge `inst.polyphony`.

## Impact

- **Engine / Runtime C++ (`native/src/core/event_instance.h`, `native/src/core/event_instance.cpp`)**: Correção de resolução de destino, suporte completo a `TransitionMarker`, crossfade em `sync_timeline_jump()` e roubo do grão mais antigo em `spawn_scatterer_grain()`.
- **Testes C++ (`native/tests/timeline_logic.cpp`)**: Atualização de fixtures para `LogicItemKind::Marker` e adição de testes de regressão/paridade para `TransitionMarker`, roundtrip via `oae_bankc`, `crossfade_ms` e `Scatterer` polyphony stealing.
- **ABI C / Formato `.bank` / Studio UI / Unity**: Nenhum impacto ou quebra; os campos necessários (`crossfade_ms`, `quantization`, `polyphony`, `LogicItemKind::Marker`, `LogicItemKind::TransitionMarker`) já existem na ABI e no formato binário.
