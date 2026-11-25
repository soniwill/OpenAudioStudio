# Verificação — fix-bank-sync-audio-table (2026-11-16)

## Gates

- `openspec validate fix-bank-sync-audio-table --type change --strict`: **PASS** (exit 0).
- `./scripts/verify.ps1 -Layer native` (Debug e Release): **PASS** — CTest 2/2 em cada
  configuração, `RESULT: OK` (`native-gate.log`).
- Teste novo `bank_sync_audio_table` (caminho síncrono da API C, não `BankReader`):
  doctest 1 caso, 33 asserções, `Status: SUCCESS` (`native-test-bank-sync.txt`).
  Confirma: `audio_table` presente no `ModelSet` reconstruído, `sample_index == 0`,
  `sound_handle` rebindado, Programmer reproduz audível sem callback.
- `./scripts/verify.ps1 -Layer studio` (change da semente): todos os smokes Release
  passaram, incluindo `--smoke-authoring` ("Programmer, audio_table and mixed playlist
  verified") e bundle recriado.

## Validação na interface (MCP, cópia temporária do projeto)

Studio Release do bundle `build/Studio_Bundle_Release` (binário do gate studio), aberto
com `--mcp` sobre cópia temporária em `%TEMP%\opencode\oas-bugfix-ui\example_project`
com `audio_table` (`coin_demo` → `sample_coin`) e `event:/sfx/programmer_demo`
(instrumento programmer, 500 ms). Porta verificada com `scripts/mcp_call.js --check`.

- `studio_build_bank` (`ui-build-bank.json`): "Bank built and hot-swapped
  successfully" — a prévia usa o caminho de carga reconstruído (o mesmo do bug).
- `studio_transport` play em `event:/sfx/programmer_demo` + capturas `deck_mixer`
  (`prog-baseline-stopped.png`, `prog-f1-playing.png`, `prog-f2-playing.png`):
  diff de pixels contra o baseline parado = **310 e 280 pixels** (medidores acesos).
  No build com o bug, o FINDINGS-1.4 registrou **zero** movimento em 10 quadros.
- `programmer-meters-16plays.json`: 16 plays independentes do Programmer, todos com
  burst audível em `bus:/sfx` (rms −8 dB durante ~300 ms), via leitura do recurso
  `oae://audio/meters` — coletados com o mesmo harness da change da semente.
- Controle negativo da metodologia: screenshot parado vs baseline parado = 0 pixel
  (registrado na change da semente, `multi-ctrl-stopped.png`).

## Notas

- A prévia do Studio permanece em loop/hold de pico após o fim do evento; os diffs
  de pixels a 1,1 s (`prog-lp-f1100.png`) refletem hold de medidores, não loop
  perceptível do áudio — discriminados pelos medidores com janela RMS curta.
- Carga assíncrona confirmada sem o bug por leitura (`run_async_parsed_load` usa
  `set_model_set` direto do `BankReader`).
