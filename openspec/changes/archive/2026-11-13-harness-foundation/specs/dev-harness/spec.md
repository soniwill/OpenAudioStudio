# Spec Delta

## Purpose

Define a governança documental, a organização de capacidades no OpenSpec, a gestão de memória persistente de agentes, a reprodutibilidade de dependências e os gates automatizados de verificação do repositório OpenAudioStudio.

## ADDED Requirements

### Requirement: Fonte canônica incremental de especificações
O projeto MUST utilizar `openspec/specs/` como fonte canônica de requisitos para todas as capacidades migradas e `openspec/changes/` para qualquer novo trabalho ou refinamento, mantendo `docs/legacy/` e `docs/specs/` congelados em modo somente leitura até a migração de cada capacidade.

#### Scenario: Proposta de novo requisito ou refinamento de capacidade
- **WHEN** um desenvolvedor ou agente iniciar o refinamento de uma parte do sistema ou propuser um requisito novo
- **THEN** o trabalho MUST ser registrado como uma change em `openspec/changes/<change-id>/` atualizando `openspec/specs/<capacidade>/spec.md`
- **AND** nenhum arquivo em `docs/legacy/` ou `docs/specs/` pode ser editado para adicionar requisitos novos

#### Scenario: Consulta a capacidade ainda não migrada
- **WHEN** uma capacidade ainda não possuir `openspec/specs/<capacidade>/spec.md`
- **THEN** o índice `docs/specs/README.md` MUST indicar quais documentos legados em `docs/specs/` e seções de `docs/legacy/blueprint.md` detêm o contrato vigente daquela capacidade

### Requirement: Taxonomia híbrida de capacidades e prefixos de escopo
As capacidades em `openspec/specs/` MUST seguir a divisão híbrida entre capacidades de conceito (alinhadas ao FMOD Studio 2.04) e capacidades de plataforma (mecanismos transversais), e cada requisito dentro de uma capacidade de conceito MUST iniciar seu título com um dos prefixos `Runtime —`, `Autoria —` ou `Unity —`.

#### Scenario: Escrita de requisitos em spec de conceito
- **WHEN** um spec de conceito (como `events`, `instruments` ou `parameters`) definir requisitos de motor, de interface do Studio e de integração com a Unity
- **THEN** cada bloco `### Requirement:` MUST usar o prefixo `Runtime —`, `Autoria —` ou `Unity —` correspondente à camada observável

#### Scenario: Escrita de requisitos em spec de plataforma
- **WHEN** um requisito tratar exclusivamente de mecanismo transversal (ABI C, formato binário `.bank`, transação do `oae_bankc`, shell do Studio, ponte UPM C#, servidor MCP ou harness)
- **THEN** ele MUST residir na capacidade de plataforma correspondente (`engine-core`, `c-api-abi`, `bank-format`, `project-publish`, `studio-shell`, `unity-bridge`, `mcp-automation` ou `dev-harness`)

### Requirement: Idioma e formatação normativa dos artefatos OpenSpec
Todos os artefatos em `openspec/` MUST ser redigidos em português (`pt-BR`), preservando os cabeçalhos estruturais do OpenSpec (`Purpose`, `Requirements`, `Requirement`, `Scenario`, `WHEN`, `THEN`, `AND`) e os verbos normativos `SHALL` e `MUST` em inglês e em letras maiúsculas.

#### Scenario: Validação estrita de change ou spec
- **WHEN** `openspec validate --strict` for executado sobre uma change ou spec do projeto
- **THEN** a validação MUST concluir com zero erros e zero avisos

### Requirement: Memória operacional enxuta separada do histórico
O arquivo `docs/ai/memory.md` MUST conter apenas o estado operacional vigente com no máximo 100 linhas (autorização atual, status por capacidade/épico, decisões recentes, pendências reais verificadas e próximo passo), enquanto relatórios e diários históricos de tarefas concluídas MUST permanecer arquivados em `docs/ai/history/`.

#### Scenario: Atualização de memória ao encerrar tarefa ou change
- **WHEN** uma tarefa autorizada ou change for concluída
- **THEN** `docs/ai/memory.md` MUST ser atualizado sem ultrapassar 100 linhas
- **AND** evidências detalhadas de execução MUST ficar registradas na change correspondente em `openspec/changes/` em vez de expandir `docs/ai/memory.md`

### Requirement: Autorização explícita de execução
Nenhum agente SHALL iniciar ou continuar tarefas de implementação do backlog ou aplicar uma change sem autorização explícita do usuário na sessão atual enquanto o avanço automático estiver marcado como suspenso em `AGENTS.md`.

#### Scenario: Sessão iniciada com avanço automático suspenso
- **WHEN** `AGENTS.md` declarar o avanço automático suspenso e o usuário não tiver autorizado implementação na sessão
- **THEN** o agente MUST limitar-se à análise, exploração ou escopo documental solicitado e aguardar autorização explícita antes de alterar código

### Requirement: Reprodutibilidade da dependência JUCE via submódulo Git
A árvore do framework JUCE utilizada pelo OpenAudio Studio em `studio/juce` MUST ser rastreada como submódulo Git fixado no commit correspondente à versão homologada (`8.0.12`), permitindo que um clone recursivo limpo configure e compile o alvo `oae_studio` sem cópias manuais externas.

#### Scenario: Verificação de submódulo em clone ou gate de Studio
- **WHEN** o desenvolvedor ou script de verificação inspecionar a dependência `studio/juce`
- **THEN** `.gitmodules` MUST declarar `studio/juce` apontando para o repositório oficial do JUCE
- **AND** `studio/juce/CMakeLists.txt` MUST estar presente no commit fixado antes de invocar o build do Studio

### Requirement: Gates automatizados de verificação por camada e no arquivamento
O repositório MUST fornecer o script `scripts/verify.ps1` que executa os gates de validação correspondentes às camadas modificadas (`native`, `studio`, `unity`, `docs`) com código de saída zero em sucesso e não-zero em falha, exigindo o gate completo (`-All`) antes do arquivamento de qualquer change.

#### Scenario: Verificação incremental por camada modificada
- **WHEN** `scripts/verify.ps1` for executado durante o desenvolvimento de uma change
- **THEN** ele MUST executar os gates das camadas selecionadas ou detectadas no `git diff` (`build.ps1` Debug+Release para `native`, `build_studio.ps1` Release para `studio`, `unity_test.ps1` para `unity`, e validação OpenSpec + links/higiene para `docs`) e encerrar com `RESULT: OK` (exit 0) ou `RESULT: FAILED` (exit 1)

#### Scenario: Gate completo para arquivamento de change
- **WHEN** uma change estiver pronta para ser arquivada via `openspec archive`
- **THEN** `scripts/verify.ps1 -All` MUST ser executado
- **AND** caso algum gate de ambiente externo (como o Editor da Unity ou loopback de rede) esteja indisponível, o status `SKIPPED`/`PENDING` MUST bloquear o arquivamento até que o usuário autorize explicitamente a dispensa
