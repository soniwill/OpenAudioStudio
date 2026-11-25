# Tasks

## 1. Correção do caminho de carga síncrona

- [x] 1.1 Copiar a `audio_table` do banco lido para o builder na reconstrução do `ModelSet` em `native/src/api/bank_api.cpp`, cobrindo também bancos v1 sem tabela (ausência permanece vazia). Verificar por leitura do código que o builder recebe as entradas chave → `sample_id`.
- [x] 1.2 Adicionar teste nativo em `native/tests/` que carregue um banco com `audio_table` pelo caminho síncrono da API C (não pelo `BankReader` direto) e confirme que o `ModelSet` reconstruído contém as entradas da tabela e que um Programmer sem callback resolve a chave e reproduz o sample. Verificar com o novo teste passando em Debug e Release (`./scripts/verify.ps1 -Layer native`).

## 2. Verificação e evidências

- [x] 2.1 Rodar `./scripts/verify.ps1 -Layer native` (CTest Debug e Release) e registrar resultado em `evidence/`. *(Aprovado 2026-11-16: CTest Debug 2/2 e Release 2/2, `RESULT: OK` — `evidence/native-gate.log`; testes novos `bank_sync_audio_table` (33 asserções) e `playlist_random_seed` (6 casos/1040 asserções) passaram individualmente.)*
- [x] 2.2 Validar na interface, com o Studio Release e `scripts/mcp_call.js --check` (cóppia do projeto com evento Programmer, ex.: `programmer_demo` da tarefa 1.4), que a prévia do Programmer acende os medidores com a chave da tabela. Registrar evidência (`--out`) na change. *(Aprovado 2026-11-16: bundle Release com `--mcp`, `studio_build_bank` hot-swap, play do `programmer_demo` com diff de 310/280 pixels vs baseline parado (zero no build com o bug) e 16 plays audíveis em `bus:/sfx` via `oae://audio/meters` — ver `evidence/VERIFICATION.md`.)*
- [x] 2.3 Rodar `openspec validate fix-bank-sync-audio-table --type change --strict` e registrar resultado. *(Aprovado 2026-11-16: exit 0 — `evidence/validate-strict.txt`.)*

## Workflow follow-up

- Desbloqueada a tarefa 1.4 de `studio-help-workflow-accuracy` (exercício de Programmer) — retomá-la em change separada, após autorização.
- Arquivar após gates e evidências completos, sincronizando o requisito em `openspec/specs/instruments/spec.md`.
