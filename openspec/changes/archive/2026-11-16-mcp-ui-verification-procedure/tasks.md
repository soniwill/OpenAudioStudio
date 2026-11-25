# Tasks

## 1. Cliente direto e procedimento

- [x] 1.1 Escrever `scripts/mcp_call.js`: cliente JSON-RPC 2.0 newline-delimited sobre TCP `127.0.0.1:7451` (`OAE_MCP_HOST`/`OAE_MCP_PORT`), com `--check` (handshake e exit 0), `--list` (catálogo), chamadas `<ferramenta> '<json>'|@arquivo.json` (argumento por arquivo com stripping de BOM), `--out` para salvar a resposta completa, `--wait` para aguardar a porta, `--timeout` de inatividade e códigos de saída distintos (0 sucesso, 1 conexão, 2 erro da ferramenta).
- [x] 1.2 Validar o script contra o Studio em execução com servidor MCP: `--check` exit 0, `--list` exit 0 e chamada real `studio_ui_inspect @args.json --out` com `isError: false`, `diff: []` e resposta salva, sem tocar mouse/teclado na janela do usuário.

## 2. Prompts dos agentes e documentação

- [x] 2.1 Adicionar ao prompt do `oas-orquestrador` (`.opencode/orquestrador.md`) o item "Verificar na interface (MCP)": checagem prévia com `node scripts/mcp_call.js --check`, abertura com `--mcp` somente sem instância em execução, uso do script quando o catálogo da sessão não expuser `studio_ui_*` e delegação de cenários deterministas ao `oas-verificacao`.
- [x] 2.2 Atualizar o system do `oas-verificacao` em `opencode.json`: exigir do coordenador o Studio aberto, checar com `--check`, chamar via `node scripts/mcp_call.js <ferramenta> '<json>'|@arquivo.json` com `--out` quando o catálogo não expuser o namespace.
- [x] 2.3 Documentar em `docs/ai/opencode.md` (seção MCP do Studio) a precondição de catálogo por sessão e o caminho de fallback com `scripts/mcp_call.js`.
- [x] 2.4 Confirmar o recarregamento no serviço OpenCode 2.3.0 ativo: system do orquestrador e do `oas-verificacao` consultados via API com o procedimento presente.

## 3. Formalização OpenSpec

- [x] 3.1 Criar a change `mcp-ui-verification-procedure` com proposal, delta spec de `mcp-automation` (ADDED requirement com os seis cenários) e design, e validar com `openspec validate mcp-ui-verification-procedure --type change --strict`.
- [x] 3.2 Registrar na memória operacional (`docs/ai/memory.md`) a origem do script, o procedimento e a formalização, dentro do limite de 100 linhas do gate docs.
- [x] 3.3 Após revisão, sincronizar o delta para o spec canônico `openspec/specs/mcp-automation/spec.md` e arquivar a change dentro da autorização, com `./scripts/verify.ps1 -Layer docs` aprovado.