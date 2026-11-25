# Verificação 1.4 (2026-11-15/16) — Multi, Scatterer e Programmer

Studio Release do bundle `build/Studio_Bundle_Release` (mesmo binário das gates),
cópia de trabalho em `%TEMP%\opencode\oas-help14\example_project` com eventos de
teste `event:/sfx/multi_demo`, `event:/sfx/scatterer_demo` e
`event:/sfx/programmer_demo` (um instrumento por evento) e
`audio_table` `[{"key":"coin_demo","sample_id":"sample_coin"}]`.
Evidências: PNGs de `deck_mixer` por diff de pixels contra o baseline parado
(metodologia calibrada com `event:/sfx/coin`, que acende master e sfx).

## Resultados

- Playlist preenchida (Multi): confirmada por `studio_ui_inspect`
  (`05-playlist-deck.json`) e captura `06-playlist-multi-filled.png` — modo
  `Random (weighted)`, duas entradas (sample + evento aninhado), peso 80%.
- Scatterer: 6 disparos audíveis com intervalos variáveis (~1,4 s de janela),
  silêncio após o total (`scatter-f1..f12.png`).
- Multi 80/20: 14 plays consecutivos sortaram a moeda em todos; nenhuma
  escolha do tema observada. Sorteio determinístico entre plays (ver bug 2).
- Programmer com `coin_demo`: `studio_transport` play executou com sucesso,
  mas nenhum movimento de medidor em 10 quadros (`prog3-f1..f10.png`);
  controle positivo `event:/sfx/coin` acendeu os medidores na mesma sessão.

## Bugs de engine descobertos (fora do escopo desta change)

1. **Carga síncrona do banco descarta a `audio_table`.**
   `native/src/api/bank_api.cpp:45-74` revincula `sound_handle` de
   instrumentos e entradas de playlist ao reconstruir o `ModelSet`, mas não
   copia a `audio_table` para o builder; a tabela fica vazia no runtime, o
   fallback em `native/src/core/event_instance.cpp:1161-1165` não encontra a
   chave e o Programmer silencia. O round-trip do compilador
   (`native/tests/bank_compiler.cpp:542`) não pega porque lê o banco direto
   pelo `BankReader`, sem passar por essa reconstrução. O ATBL correto existe
   no binário (`coin_demo` presente via `oae_bankc --verbose`).
2. **Semente fixa por `EventDescription` no sorteio global da playlist.**
   Cada Play cria uma nova `EventDescription`
   (`native/src/api/event_api.cpp:28`) e o `PlaylistState` global nasce com
   semente fixa (`native/src/core/studio_model.h:861`,
   `Pcg32(0x12345678, 0x9abcdef0)`); a primeira escolha de cada play é sempre
   igual, tornando o exercício "pré-visualize mais de uma vez para ouvir a
   diferença" não verificável neste build.

## Resolução dos bugs e atualização do manual (2026-11-16)

Ambos os bugs de engine foram corrigidos, validados e arquivados em changes dedicadas:
1. **`fix-bank-sync-audio-table`**: Cópia da `audio_table` implementada em `native/src/api/bank_api.cpp`. Teste de carga síncrona `bank_sync_audio_table` e validação de prévia no Studio Release com 16 plays audíveis e medidores acendendo em `bus:/sfx`. Requisito sincronizado em `instruments/spec.md`.
2. **`fix-playlist-random-seed`**: `make_playlist_rng()` implementado em `native/src/core/studio_model.h` com semente central atômica/temporal. Teste estatístico 400 plays (3σ) e validação de 40 plays independentes na UI (29 moedas / 11 temas por duty cycle de `oae://audio/meters`). Requisito sincronizado em `instruments/spec.md`.

Com os comportamentos de runtime corrigidos e aprovados no gate completo (`./scripts/verify.ps1 -All`), os capítulos 6, 7 e 11 do manual offline, `manual.js` e `docs/manual/PLANO.md` foram devidamente atualizados, refletindo a autoria de Multi e Scatterer, a prevenção anti-ciclo e a reprodução auditiva confirmada do Programmer Instrument via Audio Table.
