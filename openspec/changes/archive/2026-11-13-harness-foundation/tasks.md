# Tasks

## 1. Configuração do OpenSpec e Higiene da Árvore (`docs`)

- [x] 1.1 Atualizar `openspec/config.yaml` com o `context` do projeto (`pt-BR`, cabeçalhos e `SHALL`/`MUST` em inglês, taxonomia híbrida das 19 capacidades, prefixos `Runtime —`/`Autoria —`/`Unity —`, legado somente leitura) e as `rules` por artefato (`proposal`, `specs`, `tasks`), verificando com `openspec validate "harness-foundation" --strict`.
- [x] 1.2 Atualizar `.gitignore` para ignorar `debug.log` e `example_project/build/preview.bank*` e remover `studio/juce/` da lista de ignorados; remover do disco os artefatos efêmeros não rastreados (`debug.log`, `example_project/build/preview.bank`, `example_project/build/preview.bank.catalog.json`) e verificar que `git status --short` não lista mais arquivos temporários.

## 2. Isolamento do Legado e Índice de Rastreabilidade (`docs`)

- [x] 2.1 Mover os dois blueprints da raiz via `git mv` para `docs/legacy/blueprint.md` (versão `_fix.md`) e `docs/legacy/blueprint-original.md` (versão histórica), criar `docs/legacy/README.md` explicando o congelamento somente leitura e o mapeamento de seções para capacidades OpenSpec, e verificar que nenhum arquivo `*Blueprint*.md` permanece na raiz.
- [x] 2.2 Criar `docs/specs/README.md` listando o mapa completo das 19 capacidades OpenSpec (11 de conceito FMOD + 8 de plataforma) e uma tabela que associe cada um dos arquivos existentes em `docs/specs/` à sua capacidade alvo e status (`Legado — aguardando migração`), verificando que todos os arquivos de `docs/specs/` estão indexados.

## 3. Reestruturação da Memória e Regras de IA (`docs`)

- [x] 3.1 Criar `docs/ai/history/`, copiar o conteúdo integral atual de `docs/ai/memory.md` para `docs/ai/history/memory-2026-11-12.md`, mover `docs/ai/review-fix.md` e `docs/ai/status-review.md` via `git mv` para `docs/ai/history/`, e verificar que os três arquivos existem intactos em `docs/ai/history/`.
- [x] 3.2 Reescrever `docs/ai/memory.md` com no máximo 100 linhas contendo autorização atual (suspensa), estado real consolidado por épico/capacidade, decisões vigentes (`docs/decisions/001` e `docs/decisions/002`), pendências técnicas reais reconciliadas e próximo passo, verificando que `(Get-Content docs/ai/memory.md).Count -le 100`.
- [x] 3.3 Atualizar `docs/ai/rules.md`, `AGENTS.md` e `README.md` para refletir o caminho `docs/legacy/blueprint.md`, a regra de criação de changes em `openspec/changes/` em vez de novos arquivos em `docs/specs/`, a visão atualizada do projeto (Engine + Studio + Unity + MCP) e os comandos de build/verificação, verificando que todos os links relativos desses três arquivos apontam para caminhos existentes.

## 4. Submódulo JUCE e Script Unificado de Verificação (`studio` / `docs`)

- [x] 4.1 Registrar `studio/juce` (`https://github.com/juce-framework/JUCE.git`, commit `29396c22c93392d6738e021b83196283d6e4d850`, tag `8.0.12`) em `.gitmodules` e como gitlink no índice do Git sem recompilar nem perder a árvore local, verificando com `git submodule status studio/juce`.
- [x] 4.2 Criar `scripts/verify.ps1` suportando `-Layer <docs|native|studio|unity>`, detecção automática por `git diff` quando `-Layer` é omitido e `-All` para gate de arquivamento (incluindo checagem do submódulo JUCE, `openspec validate --all --strict`, limite de 100 linhas de `docs/ai/memory.md` e verificação de links nos arquivos de entrada), e verificar sua execução com `./scripts/verify.ps1 -Layer docs` retornando `RESULT: OK` e exit code `0`.

## Workflow follow-up

- Executar `./scripts/verify.ps1 -All` antes de arquivar a change (`openspec archive "harness-foundation"`), solicitando decisão explícita do usuário caso algum gate de ambiente externo precise de dispensa.
