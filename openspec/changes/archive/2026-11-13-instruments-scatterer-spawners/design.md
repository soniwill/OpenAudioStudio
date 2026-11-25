## Context

`InstrumentKind` em `native/src/core/studio_model.h` atualmente define `Single (0)`, `Multi (1)`, `Event (2)`, `Command (3)`, `Silence (4)` e `Programmer (5)`. `Multi` já possui `PlaylistEntry` (`sample_index` / `event_guid` + `weight`) e `PlaylistState` com `PCG32` em `EventInstance`. Entretanto, `bank_reader.cpp` (linha 266) valida `kind_u8 <= 4` na leitura padrão do chunk `EVTS`, enquanto `Programmer` e campos estendidos usam metadados auxiliares.

## Goals / Non-Goals

**Goals:**
- Introduzir `InstrumentKind::Scatterer = 6` preservando compatibilidade binária com `.bank` v1 existentes.
- Reutilizar `PlaylistEntry`, `PlaylistMode`, `PlaylistScope` e `PlaylistState` para o sorteio de grãos (samples ou eventos filhos).
- Implementar scheduler de spawner por instância em `EventInstance` com atualização por delta de tempo em `update()`, controle estrito de `polyphony` / `total_sounds` e dispersão 2D determinística (`pan` + atenuação por distância).
- Entregar autoria completa no Studio (`AuthoringModel`, `InspectorPanel`, `PlaylistDeck`, `TimelineTrackLane`).

**Non-Goals:**
- Modulação dinâmica de `spawn_rate` / `spawn_interval` por parâmetros de evento (ficará para integração com `modulation-automation-ui`).
- Atenuação física 3D completa com oclusão/cone (Épico 10 / `spatialization-3d-runtime`).

## Decisions

1. **Modelo e Serialização Retrocompatível (`.bank`):**
   - Em `studio_model.h`, adicionar `InstrumentKind::Scatterer = 6` e struct `ScattererSettings` em `Instrument`:
     - `double spawn_interval_min_ms = 200.0;`
     - `double spawn_interval_max_ms = 1000.0;`
     - `std::uint32_t polyphony = 4;`
     - `std::uint32_t total_sounds = 0;` (`0` = infinito enquanto ativo)
     - `float min_scatter_distance = 1.0f;`
     - `float max_scatter_distance = 10.0f;`
   - Em `bank_writer.cpp` / `bank_reader.cpp`: ampliar `kind_u8` aceito no `EVTS` (ou gravar `ScattererSettings` em sub-bloco opcional ao final do `EVTS` com marcador/versão segura, mantendo leitura 100% compatível para bancos sem `Scatterer`).

2. **Scheduler de Grãos em `EventInstance`:**
   - Adicionar `ActiveScattererState` por `(track_idx, inst_idx)` contendo `active` flag, `next_spawn_countdown_ms`, `spawned_count`, e rastrear os canais/filhos pertencentes ao spawner para contar polifonia ativa (`oae_channel_is_playing` / `child->is_active()`).
   - No disparo inicial (offset atingido ou condição válida), disparar o primeiro grão (se `polyphony > 0`) e sortear `next_spawn_countdown_ms` em `[spawn_interval_min_ms, spawn_interval_max_ms]` via `rng_`.
   - Em `EventInstance::update()`, descontar `delta_ms` de `next_spawn_countdown_ms` enquanto o spawner estiver ativo e abaixo de `total_sounds`; ao zerar, se `active_grains < polyphony`, disparar próximo grão (sample via `trigger_instrument` ou evento via `trigger_event_instrument`) com `pan` sorteado em `[-1, 1]` e atenuação `1.0f / (1.0f + dist * 0.1f)` normalizada relativa a `min_scatter_distance`.

3. **Autoria no Studio:**
   - `AuthoringModel::add_instrument(..., "scatterer")` cria instrumento com defaults (`offset_ms: 0`, `length_ms: 2000`, `playlist_mode: "random"`, `playlist_scope: "instance"`, `playlist: []` e objeto/campos `spawn_interval_min_ms`, `spawn_interval_max_ms`, `polyphony`, `total_sounds`, `min_scatter_distance`, `max_scatter_distance`), além de helper `set_scatterer_properties(...)`.
   - `InspectorPanel` exibe sliders/campos numéricos para as propriedades de spawner quando `kind == "scatterer"`.
   - `PlaylistDeck` trata `kind == "scatterer"` da mesma forma que `kind == "multi"`.
   - `TimelineTrackLane` renderiza `[SCT]` com cor distinta (`0xff2a9d8f`).

## Risks / Trade-offs

- **Bancos antigos vs novos:** garantir que `bank_reader.cpp` só leia o bloco extra de `Scatterer` quando `evts_r.remaining() > 0` (ou via sub-chunk explícito), evitando quebrar bancos v1 gerados antes desta change.
- **Limpeza de vozes terminadas:** podar canais já finalizados antes de checar `polyphony` evita que vozes mortas bloqueiem novos grãos.
