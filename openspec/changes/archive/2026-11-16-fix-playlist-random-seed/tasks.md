# Tasks

## 1. Semeadura do PlaylistState

- [x] 1.1 Substituir a semente fixa `Pcg32(0x12345678, 0x9abcdef0)` do `PlaylistState` global em `native/src/core/studio_model.h` por semeadura a partir de um gerador central não determinístico (contador atômico + `steady_clock`/`random_device`, com fallback ao contador), conforme design.md. Verificar por leitura do código que o estado de instância (escopo Instance) continua determinístico dentro da mesma instância.
- [x] 1.2 Buscar dependências de teste na semente fixa (`rg 0x12345678 0x9abcdef0 native/`) e ajustar qualquer teste que assuma a primeira escolha constante, mantendo-o estável (comportamento de instância única ou afirmações estatísticas com margem).

## 2. Testes e verificação

- [x] 2.1 Adicionar teste nativo em `native/tests/` com N plays independentes de um Multi `Random (weighted)` 80/20 exigindo: ambas as entradas selecionadas ao menos uma vez e frequência da entrada majoritária dentro de 3 desvios padrão binomiais. Teste passa em Debug e Release (`./scripts/verify.ps1 -Layer native`).
- [x] 2.2 Rodar `./scripts/verify.ps1 -Layer native` (CTest Debug e Release) e `./scripts/verify.ps1 -Layer studio` (smokes Release), registrando resultados em `evidence/` — o gate do Studio cobre playlist via smokes existentes. *(Aprovado 2026-11-16: native Debug 2/2 e Release 2/2, `RESULT: OK` — `evidence/native-gate.log`; studio `RESULT: OK` com todos os smokes Release (headless, project, authoring, timeline, transport-mixer, end-to-end, device, pro-ui, mcp, vu, live) e bundle recriado — `evidence/studio-gate.log`.)*
- [x] 2.3 Validar na interface, com o Studio Release e `scripts/mcp_call.js --check` (cópia do projeto com `multi_demo` 80/20), que plays repetidos selecionam entradas distintas (medidores). Registrar evidência (`--out`) na change. *(Aprovado 2026-11-16: 40 plays independentes, 29 moeda/11 tema por duty cycle de `oae://audio/meters` (72,5%/27,5%, dentro de 3σ do 80/20), plays distintos entre si vs sorteio idêntico no build com o bug — ver `evidence/VERIFICATION.md`.)*
- [x] 2.4 Rodar `openspec validate fix-playlist-random-seed --type change --strict` e registrar resultado. *(Aprovado 2026-11-16: exit 0 — `evidence/validate-strict.txt`.)*

## Workflow follow-up

- Desbloqueada a parte de Multi da tarefa 1.4 de `studio-help-workflow-accuracy` — retomá-la em change separada, após autorização.
- Arquivar após gates e evidências completos, sincronizando o requisito em `openspec/specs/instruments/spec.md`.
