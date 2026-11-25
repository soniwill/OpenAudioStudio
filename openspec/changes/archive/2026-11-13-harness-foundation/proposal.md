# Proposal

## Why

O repositório cresceu rapidamente até os Épicos 7 e 11 acumulando três sistemas concorrentes de especificação (blueprint monolítico de 105 KB com nome de arquivo corrompido na raiz, ~45 documentos heterogêneos em `docs/specs/` e uma instalação vazia do OpenSpec), uma memória persistente de 96 KB que virou diário de execução com contradições sobre o próximo passo autorizado, uma dependência crítica (`studio/juce`) ignorada no Git sem mecanismo reproduzível de clone e gates de validação acionados manualmente sem regra objetiva por camada. Antes de refinar as capacidades de produto alinhadas ao FMOD Studio 2.04, é necessário organizar o harness documental, de memória, de dependências e de verificação do projeto.

## What Changes

- Estabelece o **OpenSpec** (`openspec/specs/` e `openspec/changes/`) como fonte canônica incremental dos requisitos, estruturado em **19 capacidades híbridas**: 11 de conceito alinhadas ao FMOD Studio 2.04 (`events`, `instruments`, `timeline-logic`, `parameters`, `modulation-automation`, `mixing`, `snapshots-vcas`, `spatialization`, `banks-assets`, `live-update`, `metering-profiling`) e 8 de plataforma (`engine-core`, `c-api-abi`, `bank-format`, `project-publish`, `studio-shell`, `unity-bridge`, `mcp-automation`, `dev-harness`).
- Configura `openspec/config.yaml` com contexto arquitetural, idioma `pt-BR` (mantendo cabeçalhos estruturais OpenSpec e `SHALL`/`MUST` em inglês), prefixos obrigatórios em specs de conceito (`Runtime —`, `Autoria —`, `Unity —`) e regras por artefato.
- Congela o legado documental: move os dois blueprints da raiz (com nomes truncados/duplicados) via `git mv` para `docs/legacy/blueprint.md` e `docs/legacy/blueprint-original.md` com `docs/legacy/README.md`, e cria `docs/specs/README.md` mapeando cada arquivo legado para sua capacidade OpenSpec alvo e status de migração.
- Reestrutura a memória persistente: preserva o histórico integral em `docs/ai/history/` (junto com `review-fix.md` e `status-review.md`) e reescreve `docs/ai/memory.md` como resumo canônico enxuto (<= 100 linhas) refletindo o estado real verificado, a suspensão do avanço automático e as pendências reais reconciliadas.
- Alinha `AGENTS.md`, `docs/ai/rules.md`, `README.md` e `.gitignore` ao fluxo OpenSpec e à higiene da árvore de trabalho (`debug.log`, `example_project/build/preview.bank*`).
- Torna a obtenção do JUCE 8.0.12 (`29396c22c93392d6738e021b83196283d6e4d850`) reproduzível como submódulo Git em `studio/juce`.
- Introduz o script unificado `scripts/verify.ps1` implementando gates por camada tocada (`native`, `studio`, `unity`, `docs`) para validação rápida durante uma change e gate completo (`-All`) exigido antes do arquivamento.

## Capabilities

### New Capabilities
- `dev-harness`: Governança de especificações (transição incremental para OpenSpec e mapa híbrido de 19 capacidades), gestão de memória persistente e legado documental, reprodutibilidade de dependências e gates automatizados de verificação por camada.

### Modified Capabilities

## Impact

- **Documentação e Governança de IA**: `AGENTS.md`, `README.md`, `openspec/config.yaml`, `docs/ai/memory.md`, `docs/ai/rules.md`, `docs/ai/history/*`, `docs/legacy/*`, `docs/specs/README.md`.
- **Scripts e Automação de Build/Teste**: Novo `scripts/verify.ps1` orquestrando `scripts/build.ps1`, `scripts/build_studio.ps1`, `scripts/unity_test.ps1` e verificação documental/OpenSpec.
- **Controle de Versão e Dependências**: `.gitignore`, `.gitmodules` e registro do submódulo `studio/juce` no commit `29396c22c93392d6738e021b83196283d6e4d850` (tag `8.0.12`).
- **Código de Produção**: Nenhuma alteração funcional no motor C/C++, no código-fonte do Studio nem no pacote C# da Unity.
