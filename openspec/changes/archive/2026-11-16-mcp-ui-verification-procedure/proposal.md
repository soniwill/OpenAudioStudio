# Proposal

## Why

A validação de changes na interface do Studio via MCP (`studio_ui_*`) dependia de improviso em runtime: as ferramentas MCP carregam somente no início da sessão do OpenCode, e quando a sessão nasce com o Studio fechado o namespace não existe — o agente descobre isso no meio da tarefa, improvisa uma sonda JSON-RPC ad hoc e consome dezenas de chamadas refazendo infraestrutura de evidência (episódio registrado na aplicação de `fix-studio-timeline-automation-point-deletion` em 2026-11-16). O procedimento precisa ser determinístico: checar a porta antes, abrir o Studio quando fechado e usar um cliente documentado do repositório como caminho de fallback disponível a qualquer agente, incluindo `oas-verificacao`.

## What Changes

- Promover o cliente direto do servidor MCP do Studio a script do repositório: `scripts/mcp_call.js` (JSON-RPC 2.0 newline-delimited sobre TCP `127.0.0.1:7451`), com `--check` (verificação de conectividade), `--list` (catálogo de ferramentas), chamadas com argumentos por linha (`'<json>'`) ou por arquivo (`@arquivo.json`, contorna o quoting do PowerShell 5.1) e `--out` para salvar a resposta completa como evidência.
- Definir o procedimento de verificação na interface: antes de usar as ferramentas `studio_ui_*`, o agente confere a porta com `node scripts/mcp_call.js --check`; se fechada e sem instância em execução, abre o Studio com `--mcp` e aguarda a porta; quando o catálogo da sessão não expõe o namespace, chama as ferramentas via `scripts/mcp_call.js` com `--out`.
- Registrar o procedimento nos prompts dos agentes (`oas-orquestrador` e `oas-verificacao`), permitindo que cenários deterministas de validação de UI sejam delegados ao subagente de verificação com passos exatos.

## Capabilities

### New Capabilities

(nenhuma)

### Modified Capabilities

- `mcp-automation`: adiciona requisito de procedimento de verificação na interface — checagem prévia de conectividade, abertura assistida do Studio com `--mcp` e cliente direto `scripts/mcp_call.js` como caminho de chamada independente do catálogo da sessão, com evidência via `--out`.

## Impact

- **Script**: `scripts/mcp_call.js` (novo; já escrito e validado contra o Studio em execução — exit 0, `isError: false`).
- **Prompts de agentes**: `.opencode/orquestrador.md` e `oas-verificacao` em `opencode.json` (já atualizados).
- **Docs**: `docs/ai/opencode.md` (seção MCP do Studio, já atualizada).
- **Especificação legada**: `docs/specs/mcp-server-guide.md` e `docs/specs/mcp-protocol.md` seguem somente leitura; o spec canônico `openspec/specs/mcp-automation/spec.md` recebe o novo requisito.
- **Sem impacto de runtime**: nenhuma mudança no motor C/C++, ABI, formato de banco ou no próprio servidor MCP do Studio.