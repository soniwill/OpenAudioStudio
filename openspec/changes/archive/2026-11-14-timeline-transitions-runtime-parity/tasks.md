# Tasks

## 1. Resolução de destinos e avaliação de TransitionMarker / TransitionRegion (Native)

- [x] 1.1 [Native] Adicionar `resolve_marker_destination_ms` e membros de estado (`pending_transition_quant_`, `pending_transition_xfade_ms_`, `pending_transition_arm_ms_`) em `native/src/core/event_instance.h` e `native/src/core/event_instance.cpp`, aceitando `LogicItemKind::Marker` e fallback `LogicItemKind::TransitionMarker` sem destino. Verificação: testes existentes e novos de salto para `Marker` passam no alvo `timeline_logic`.
- [x] 1.2 [Native] Atualizar `EventInstance::start()` para limitar `pre_scheduled_until_ms_` diante de `TransitionMarker` com destino e implementar a avaliação imediata e quantizada (`Beat`/`Bar` e condição de parâmetro) de `TransitionMarker` e `TransitionRegion` em `EventInstance::update()`. Verificação: testes em `native/tests/timeline_logic.cpp` validam `TransitionMarker` imediato/condicional, quantizado `Beat`/`Bar` e roundtrip via `.bank`.

## 2. Crossfade em saltos de transição (Native)

- [x] 2.1 [Native] Estender `EventInstance::sync_timeline_jump(double new_pos_ms, double crossfade_ms = 0.0)` em `native/src/core/event_instance.h/.cpp` para aplicar fade-out (`oae_channel_stop_with_fade`) em vozes síncronas anteriores, fade-in (`oae_channel_fade_to` `0 -> 1`) nas novas vozes síncronas e atualizar `pre_scheduled_until_ms_` após o salto. Verificação: teste em `native/tests/timeline_logic.cpp` confirma coexistência e curvas de fade-out/fade-in durante `crossfade_ms > 0` e parada limpa após o período de crossfade.

## 3. Sound Stealing Oldest no Scatterer Instrument (Native)

- [x] 3.1 [Native] Implementar `EventInstance::steal_oldest_scatterer_grain(std::size_t track_idx, std::size_t inst_idx)` e aplicá-lo em `EventInstance::spawn_scatterer_grain()` quando `count_active_scatterer_grains >= inst.polyphony` (`inst.polyphony > 0`) em `native/src/core/event_instance.h/.cpp`. Verificação: teste em `native/tests/timeline_logic.cpp` comprova que novos grãos interrompem o grão mais antigo e preservam `count <= inst.polyphony`.

## 4. Documentação e Gates de Validação (Native & Docs)

- [x] 4.1 [Native] Executar `powershell -ExecutionPolicy Bypass -File scripts/verify.ps1 -Layer native` e confirmar build limpo e 100% de aprovação na suíte CTest.
- [x] 4.2 [Docs] Executar `powershell -ExecutionPolicy Bypass -File scripts/verify.ps1 -Layer docs`, atualizar `docs/ROADMAP.md` e `docs/ai/memory.md` e validar a change com `openspec validate "timeline-transitions-runtime-parity" --strict`.
