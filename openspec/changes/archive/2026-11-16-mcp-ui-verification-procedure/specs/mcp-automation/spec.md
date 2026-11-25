# Spec Delta

## ADDED Requirements

### Requirement: Procedimento de verificação na interface com checagem prévia e cliente direto
O repositório SHALL fornecer um cliente direto do servidor MCP do Studio (`scripts/mcp_call.js`, JSON-RPC 2.0 newline-delimited sobre TCP em `127.0.0.1:7451`, configurável por `OAE_MCP_HOST`/`OAE_MCP_PORT`), e os agentes que validam a interface SHALL checar a conectividade antes de usar as ferramentas `studio_ui_*`, abrir o Studio com `--mcp` quando a porta estiver fechada e não houver instância em execução, e usar o cliente direto quando o catálogo da sessão não expuser o namespace.

#### Scenario: Checagem de conectividade com o Studio em execução
- **WHEN** o agente executa `node scripts/mcp_call.js --check` com o Studio aberto e o servidor MCP ativo
- **THEN** o comando conecta, completa o handshake `initialize` e termina com código de saída `0`

#### Scenario: Checagem com o Studio fechado produz mensagem acionável
- **WHEN** o agente executa `node scripts/mcp_call.js --check` sem instância do Studio em execução
- **THEN** o comando termina com código de saída diferente de zero e mensagem instruindo abrir o Studio com `--mcp` (ou definir `OAE_MCP_PORT`)

#### Scenario: Abertura assistida do Studio quando fechado
- **WHEN** a porta está fechada e nenhuma instância do Studio está em execução
- **THEN** o agente abre o Studio com `--mcp` e aguarda a porta aceitar conexões (via `--wait` ou sondagem do `--check`) antes de chamar as ferramentas `studio_ui_*`

#### Scenario: Preservação de instância do usuário em execução
- **WHEN** o agente precisa validar pela interface e já existe uma instância do Studio aberta
- **THEN** o agente usa essa instância e não lança outra, preservando o projeto e o estado não salvo do usuário

#### Scenario: Chamada direta de ferramenta com argumentos por arquivo e evidência
- **WHEN** o catálogo da sessão não expõe o namespace `studio_ui_*` e o agente chama `node scripts/mcp_call.js <ferramenta> @arquivo.json --out caminho.json` com o Studio aberto
- **THEN** o cliente envia `tools/call`, imprime a resposta JSON na saída, salva a resposta completa no caminho de `--out` e termina com código de saída `0` quando a ferramenta responde sem erro
- **AND** quando a ferramenta responde `isError: true` ou erro JSON-RPC, o comando termina com código de saída `2` preservando a resposta impressa e a salva em `--out`

#### Scenario: Listagem do catálogo de ferramentas
- **WHEN** o agente executa `node scripts/mcp_call.js --list` com o Studio aberto
- **THEN** o comando imprime o catálogo de ferramentas do servidor MCP com nome e descrição de cada uma