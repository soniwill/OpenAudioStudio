# Design

## Context

O servidor MCP do Studio escuta JSON-RPC 2.0 newline-delimited em TCP `127.0.0.1:7451` (transporte `stdio` também suportado, ver `docs/specs/mcp-protocol.md` e o spec canônico `mcp-automation`). No OpenCode Desktop 2.3.0, as ferramentas MCP carregam somente no início da sessão: uma sessão que nasce com o Studio fechado não ganha o namespace `studio_ui_*`, e abri-lo depois não restaura o catálogo. A aplicação de `fix-studio-timeline-automation-point-deletion` (2026-11-16) improvisou uma sonda Node ad hoc para contornar isso e consumiu ~25 chamadas refazendo infraestrutura.

O script `scripts/mcp_call.js` já foi escrito e validado contra o Studio em execução (exit 0, `isError: false`) como parte do ajuste de procedimento. Esta change formaliza o requisito e o integra ao ciclo OpenSpec.

## Goals / Non-Goals

**Goals:**
- Procedimento determinístico de verificação na interface: checar → abrir se preciso → chamar.
- Caminho de chamada independente do catálogo da sessão, disponível a qualquer agente (coordenador e `oas-verificacao`).
- Evidência de validação de UI padronizada (resposta JSON completa via `--out`).
- Permitir delegar cenários deterministas de validação de UI ao subagente de verificação com passos exatos.

**Non-Goals:**
- Mudar o servidor MCP do Studio (ferramentas, protocolo, transporte).
- Mudar o runtime C/C++, ABI, formato de banco ou a ABI pública.
- Fazer o OpenCode recarregar namespaces MCP em sessões vivas (restrição do produto 2.3.0, não do projeto).
- Automatizar login/interação não determinística (cenários de UI que dependem do usuário seguem no coordenador).

## Decisions

- **Cliente direto em `scripts/` como caminho canônico de fallback**, não plugin do OpenCode nem extensão da ponte `scripts/mcp_bridge.js`. Alternativas: (a) corrigir a ponte stdio para reconectar em runtime — não muda o catálogo da sessão, que é o problema real; (b) exigir sessão nova com Studio aberto — válida como precondição, mas não cobre o caso da sessão viva. O cliente TCP direto resolve os dois casos com um arquivo Node sem dependências.
- **Checagem prévia obrigatória (`--check`)** em vez de tentar a chamada e tratar o erro depois: falha de conexão vira mensagem acionável ("abra o Studio com `--mcp`") em vez de stack trace no meio da tarefa.
- **Abertura condicional**: abrir o Studio com `--mcp` somente quando não houver instância em execução (checar processo antes), para não lançar segunda instância sobre o projeto não salvo do usuário.
- **Argumentos por arquivo (`@arquivo.json`) como forma recomendada** no Windows: o PowerShell 5.1 degrada aspas internas de strings JSON inline e o redirecionamento `>` grava UTF-16 com BOM — ambas as falhas observadas na validação de 2026-11-16. Alternativa (escapar JSON inline) foi descartada por ser frágil.
- **Códigos de saída distintos** (0 sucesso, 1 conexão, 2 erro da ferramenta/JSON-RPC): permite ao agente e ao gate distinguir "Studio fechado" de "ferramenta reportou erro".
- **Procedimento nos prompts dos agentes**, não apenas na documentação: o orquestrador e o `oas-verificacao` recebem os passos no system, tornando o roteamento de delegação automático em vez de descoberta.

## Risks / Trade-offs

- [O catálogo da sessão não recarrega em runtime] → Procedimento documentado: sessão nova com Studio já aberto prefere as ferramentas nativas; sessão viva usa `scripts/mcp_call.js`.
- [`mcp_call.js` duplica conhecimento do protocolo] → O handshake é o mínimo (`initialize` → `tools/list`/`tools/call`), fixado no spec canônico e documentado em `docs/specs/mcp-protocol.md`; mudanças no servidor continuam compatíveis enquanto o JSON-RPC 2.0 newline-delimited se mantiver.
- [Porta padrão 7451 ocupada por outra sessão de Studio] → `OAE_MCP_PORT`/`OAE_MCP_HOST` respeitados pelo script; o orquestrador instrui abrir com `--mcp-port=<porta>` e propagar a variável quando aplicável.
- [Timeout de inatividade em chamadas longas (ex.: captura de tela grande)] → `--timeout` configurável (padrão 30 s) com mensagem acionável.