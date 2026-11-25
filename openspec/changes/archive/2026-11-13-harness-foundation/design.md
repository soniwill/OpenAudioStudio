# Design

## Context

Veja `proposal.md` para a motivação. Após o commit de checkpoint `60e0c3f`, o repositório encontra-se com o trabalho funcional preservado, mas com dívidas estruturais no harness de engenharia e IA:
1. Dois arquivos de blueprint na raiz possuem nomes corrompidos de ~160 caracteres com título duplicado e travessão Unicode (`OpenAudioEngine (OAE) — Master Blueprint, Architecture Spec & AutonomOpenAudioEngine (OAE) — Master Blueprint, Architecture Spec & Autonomous Backlogous Backlog_fix.md` e a variante sem `_fix.md`), causando escapes octais no Git e fragilidade no PowerShell/Windows.
2. `docs/ai/memory.md` acumulou 671 linhas (96 KB) misturando estado atual, decisões e logs detalhados desde a T01, além de registrar "Próximo passo autorizado: T16c" na seção final embora T16a–f, T17, Épico 6, Épico 5B, Épico 7 e Épico 11 já estejam no histórico do Git.
3. `docs/ai/rules.md` ainda instrui criar `docs/specs/<tema>.md` antes de implementar, conflitando com a adoção do OpenSpec registrada em `AGENTS.md`.
4. `studio/juce` (JUCE 8.0.12, commit `29396c22c93392d6738e021b83196283d6e4d850`) é um clone Git limpo no disco local, porém está listado no `.gitignore`; qualquer clone novo falha em `studio/CMakeLists.txt` com `FATAL_ERROR "JUCE 8.0.12 directory not found at studio/juce"`.
5. Artefatos efêmeros (`debug.log`, `example_project/build/preview.bank` e `preview.bank.catalog.json`) aparecem como untracked na árvore de trabalho.

## Goals / Non-Goals

**Goals:**
- Estabelecer uma estrutura de diretórios limpa e previsível para especificações vivas (`openspec/`), legado congelado (`docs/legacy/`, `docs/specs/`) e memória operacional (`docs/ai/`).
- Definir o mapa canônico das 19 capacidades (11 de conceito FMOD + 8 de plataforma) e a tabela de rastreabilidade do legado em `docs/specs/README.md`.
- Garantir que um clone com submódulos (`git clone --recursive`) contenha todas as dependências para compilar tanto `native` quanto `studio`.
- Fornecer um único ponto de entrada não-interativo (`scripts/verify.ps1`) para validação local por camada e futura integração em CI.

**Non-Goals:**
- Migrar neste momento os contratos técnicos de áudio (`T03`–`T25`, épicos `5B`–`12`) para `openspec/specs/` — cada capacidade será migrada incrementalmente quando for refinada.
- Configurar provedor de CI remoto ou workflows do GitHub Actions (escopo reservado para a change futura `ci-pipeline`).
- Alterar qualquer comportamento de runtime, DSP, UI do Studio ou scripts C# da Unity.

## Decisions

### 1. Estratégia de Transição para OpenSpec: Migração Incremental (Opção C)
- **Escolha**: `openspec/specs/` torna-se a fonte da verdade para capacidades já migradas; `docs/legacy/blueprint.md` e `docs/specs/` permanecem congelados como legado somente leitura até que cada capacidade seja refinada.
- **Alternativas consideradas**:
  - *A (Big-bang)*: migrar ~45 documentos de uma vez sem revisão crítica apenas copiaria ambiguidades antigas.
  - *B (Blueprint canônico)*: manteria três sistemas ativos indefinidamente.

### 2. Recorte Híbrido de 19 Capacidades + Prefixos de Camada
- **Escolha**:
  - **11 Capacidades de Conceito (FMOD Studio 2.04)**: `events`, `instruments`, `timeline-logic`, `parameters`, `modulation-automation`, `mixing`, `snapshots-vcas`, `spatialization`, `banks-assets`, `live-update`, `metering-profiling`.
  - **8 Capacidades de Plataforma**: `engine-core`, `c-api-abi`, `bank-format`, `project-publish`, `studio-shell`, `unity-bridge`, `mcp-automation`, `dev-harness`.
  - Como o schema do OpenSpec exige lista plana sob `## Requirements`, requisitos de capacidades de conceito usam os prefixos `Runtime —`, `Autoria —` e `Unity —` no título de cada `### Requirement:`.
- **Alternativas consideradas**:
  - *Por camada pura*: esconderia os conceitos de domínio do FMOD e exigiria tocar 5 specs a cada refinamento de feature.
  - *Sub-capacidades (`instruments/runtime`, `instruments/authoring`)*: dobraria o número de arquivos de spec para manter sincronizados.

### 3. Isolamento do Legado e Limpeza da Raiz
- **Escolha**:
  - `git mv` do blueprint `_fix.md` para `docs/legacy/blueprint.md` e do blueprint original para `docs/legacy/blueprint-original.md`, acompanhados de `docs/legacy/README.md`.
  - Manter `docs/specs/` no caminho atual (para não quebrar links de `docs/manual/*.html` e ADRs), adicionando `docs/specs/README.md` com o mapa completo `arquivo legado -> capacidade OpenSpec -> status`.
- **Alternativas consideradas**:
  - *Mover `docs/specs/` para `docs/legacy/specs/`*: quebraria dezenas de referências relativas no manual HTML e nos relatórios existentes.

### 4. Particionamento da Memória (`docs/ai/memory.md` vs `docs/ai/history/`)
- **Escolha**:
  - Copiar o `docs/ai/memory.md` atual (671 linhas) intacto para `docs/ai/history/memory-2026-11-12.md` e mover `docs/ai/review-fix.md` e `docs/ai/status-review.md` via `git mv` para `docs/ai/history/`.
  - Reescrever `docs/ai/memory.md` com <= 100 linhas contendo: (1) Estado e autorização, (2) Mapa resumido do que já está implementado por épico/capacidade, (3) Decisões arquiteturais vigentes (apontando para `docs/decisions/`), (4) Pendências técnicas reais verificadas no código, (5) Próximo passo (aguardando autorização do usuário).

### 5. Submódulo Git para `studio/juce`
- **Escolha**: Remover `studio/juce/` do `.gitignore` e registrar o clone existente (`https://github.com/juce-framework/JUCE.git`, tag `8.0.12`, commit `29396c22c93392d6738e021b83196283d6e4d850`) como submódulo Git em `.gitmodules` e no índice do Git (`160000`).
- **Alternativas consideradas**:
  - *CMake FetchContent*: exige rede durante configuração e perde cache ao limpar `build/`.
  - *Vendorizar 83 MB (4.377 arquivos)*: infla permanentemente o repositório Git.

### 6. Desenho de `scripts/verify.ps1`
- **Escolha**: Script PowerShell na raiz `scripts/verify.ps1` com parâmetros:
  - `-Layer <string[]>`: aceita combináveis `docs`, `native`, `studio`, `unity`. Se omitido e `-All` não for passado, infere as camadas a partir de `git status --porcelain` / `git diff --name-only HEAD`:
    - `native/**`, `CMakeLists.txt`, `scripts/build.ps1` -> `native`
    - `studio/**`, `scripts/build_studio.ps1` -> `studio`
    - `unity/**`, `scripts/unity_test.ps1`, `scripts/deploy_unity.ps1` -> `unity`
    - `docs/**`, `openspec/**`, `AGENTS.md`, `README.md`, `.gitignore`, `.gitmodules` -> `docs`
  - `-All`: ativa todas as 4 camadas (`docs`, `native`, `studio`, `unity`).
  - `-SkipUnityIfUnavailable`: permite registrar `SKIPPED` quando usado explicitamente pelo usuário; sem a flag, em `-All`, a indisponibilidade de um gate obrigatório retorna código de saída `2` (`PENDING/BLOCKED`) para impedir arquivamento silencioso.
  - Gate `docs`: executa `openspec validate --all --strict`, verifica que `docs/ai/memory.md` tem `<= 100` linhas e valida referências locais em `README.md`, `AGENTS.md` e `docs/specs/README.md`.

## Risks / Trade-offs

- **[Risco] Submódulo `studio/juce` não inicializado em clones futuros** → Mitigação: `scripts/verify.ps1` e `scripts/build_studio.ps1` verificam `studio/juce/CMakeLists.txt` e exibem mensagem clara indicando `git submodule update --init --recursive`; comando documentado no topo do `README.md`.
- **[Risco] Links históricos para o nome antigo do blueprint em `docs/ai/history/`** → Mitigação: `docs/legacy/README.md` e `docs/ai/history/` explicam o renomeio para `docs/legacy/blueprint.md`, preservando os arquivos históricos sem edição retroativa.
- **[Risco] Tempo de execução elevado quando todas as camadas rodam juntas** → Mitigação: no fluxo diário de tarefas, `scripts/verify.ps1` roda apenas as camadas tocadas; `-All` é exigido apenas no fechamento da change.

## Migration Plan

1. Configurar `openspec/config.yaml` com o contexto e regras aprovados.
2. Mover blueprints da raiz para `docs/legacy/` e criar `docs/legacy/README.md` e `docs/specs/README.md`.
3. Arquivar `memory.md` antigo, `review-fix.md` e `status-review.md` em `docs/ai/history/`, e escrever o novo `docs/ai/memory.md` enxuto e reconciliado.
4. Atualizar `docs/ai/rules.md`, `AGENTS.md`, `README.md` e `.gitignore` (removendo os arquivos temporários não rastreados `debug.log` e `example_project/build/preview.bank*`).
5. Registrar `studio/juce` como submódulo Git no commit `29396c22c93392d6738e021b83196283d6e4d850`.
6. Implementar `scripts/verify.ps1` e validar com `./scripts/verify.ps1 -Layer docs`.

## Open Questions

- **Licenciamento do JUCE 8 para distribuição futura do Studio**: JUCE 8 opera sob AGPLv3 ou licença comercial. Não afeta o harness nem o motor (`oae_audio.dll` é MIT/domínio público sem JUCE), mas deverá ser decidido antes de eventual distribuição fechada do binário `OpenAudio Studio.exe`.
- **Provedor de CI e licenciamento do Unity Editor em runners remotos**: Será decidido na change dedicada `ci-pipeline` após a criação do repositório remoto.
