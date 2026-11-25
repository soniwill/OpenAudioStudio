# Verificação — fix-playlist-random-seed (2026-11-16)

## Gates

- `openspec validate fix-playlist-random-seed --type change --strict`: **PASS** (exit 0).
- `./scripts/verify.ps1 -Layer native` (Debug e Release): **PASS** — CTest 2/2 em cada
  configuração, `RESULT: OK` (`native-gate.log`, também gravado na change do audio_table).
- Teste novo `playlist_random_seed`: doctest 6 casos, 1040 asserções, `Status: SUCCESS`
  (`native-test-playlist-seed.txt`) — 400 plays independentes 80/20 dentro de 3σ
  binomiais com ambas as entradas sorteadas, mais determinismo intra-instância com
  seed explícita.
- `./scripts/verify.ps1 -Layer studio`: **PASS** — todos os smokes Release
  (headless, project, authoring, timeline, transport-mixer, end-to-end, device,
  pro-ui, mcp, vu, live) e bundle recriado (`studio-gate.log`). Nenhum smoke
  regrediu com a nova semeadura.

## Validação na interface (MCP, cópia temporária do projeto)

Studio Release do bundle, `--mcp`, cópia temporária com `event:/sfx/multi_demo`
(Multi `random` global, playlist 80/20: `sample_coin` 0,8 / `sample_music` 0,2,
instrumento 500 ms). Medição via recurso `oae://audio/meters` (rms/peak por bus),
~22 amostras de 45 ms por play, 40 plays independentes
(`multi-meters-40plays.json`).

- Discriminador: **duty cycle** em `bus:/sfx` (janela RMS −60 dB). Moeda (300 ms)
  → duty ~0,29–0,33; tema (2000 ms truncado no instrumento de 500 ms) → duty
  ~0,41–0,52.
- Resultado: **29 plays moeda (72,5%) e 11 plays tema (27,5%)** de 40 plays.
  Binomial 80/20: esperado 32/8, σ = 2,53; observado 29 está dentro de 3σ.
- `DISTINCT = true`: plays independentes selecionam entradas distintas. No build
  com o bug (FINDINGS-1.4), 14 plays consecutivos sortearam sempre a mesma entrada
  pela semente fixa.
- Assinaturas de referência em `reference-signatures.json` (coin: pico −3,18 dB
  duty 0,27; theme: pico −3,27 dB duty 0,88 sustentado em seu próprio evento).

## Armadilhas metodológicas descobertas (documentadas para reproducibilidade)

1. A prévia do Studio permanece com medidores acesos após o fim do evento
   (hold de pico/balística) — capturas de screenshot a 1,1 s não discriminam
   entrada (`multi-play1..14.png` registram apenas "prévia ativa", não a entrada).
2. `peak_db` lido varia com a fase da leitura: o mesmo sample reproduz dois
   valores discretos de pico entre plays — classificar por pico é inválido
   (constatado em 16 plays determinísticos do Programmer).
3. O harness deve selecionar o evento explicitamente (`studio_select_item`)
   antes de medir — a seleção corrente do Studio persiste entre chamadas.

## Diff integrado

- `native/src/core/studio_model.h`: `make_playlist_rng()` inline (contador
  `std::atomic<uint64_t>` + `steady_clock`, streams PCG32 distintos por estado);
  default do `PlaylistState` chama o helper; `set_rng_seed` preservado.
- `native/tests/playlist_random_seed.cpp`: teste estatístico + determinismo.
- `native/tests/studio_events.cpp:125` semeadura explícita: inalterado e estável.
