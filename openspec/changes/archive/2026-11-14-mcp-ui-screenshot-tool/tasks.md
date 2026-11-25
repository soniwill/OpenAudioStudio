# Tasks

## 1. Catálogo e Protocolo MCP

- [x] 1.1 **[Studio / MCP]** Registrar a definição e o `inputSchema` de `studio_ui_screenshot` (`component_id`, `output_path`, `scale`, `include_base64`) em `mcp::get_tools_catalog()` (`studio/Source/MCP/McpProtocol.h`); verificar via resposta `tools/list` contendo a nova ferramenta e seus quatro parâmetros opcionais.
- [x] 1.2 **[Studio / MCP]** Declarar `tool_ui_screenshot(const nlohmann::json& args)` em `studio/Source/MCP/McpServer.h` e adicionar o roteamento em `McpServer::handle_tools_call` (`studio/Source/MCP/McpServer.cpp`), garantindo execução síncrona na JUCE Message Thread com compilação limpa no target Studio.

## 2. Implementação da Captura JUCE e Codificação PNG

- [x] 2.1 **[Studio / MCP]** Implementar a resolução do componente alvo e validação em `McpServer::tool_ui_screenshot` (`studio/Source/MCP/McpServer.cpp`), usando `ctx_.root_component` por padrão ou `find_component_recursive` para `component_id`, aplicando clamp de `scale` em `[0.25, 2.0]` (padrão `1.0`) e retornando `isError: true` para `component_id` inexistente ou área inválida (`width <= 0 || height <= 0`); verificar com chamadas válidas e inválidas no smoke test.
- [x] 2.2 **[Studio / MCP]** Renderizar o bitmap via `createComponentSnapshot`, codificar em memória com `juce::PNGImageFormat` (`juce::MemoryOutputStream`), salvar em disco quando `output_path` for informado (criando diretórios pais) e montar o array `content` com bloco `text` (JSON com `component_id`, `width`, `height`, `scale`, `byte_size` e `output_path` se aplicável) e bloco `image` base64 (`mimeType: "image/png"`) conforme regra de `include_base64`; verificar cabeçalho PNG (`89 50 4E 47`) e base64 decodificável.

## 3. Validação Smoke, Documentação e Gates

- [x] 3.1 **[Dev Harness / Smoke]** Estender `run_smoke_mcp()` em `studio/Source/Main.cpp` para exercitar: (a) `studio_ui_screenshot` na janela completa com `output_path` temporário validando arquivo PNG com assinatura `89 50 4E 47` e dimensões não nulas; (b) captura escopada em `timeline_view` retornando bloco `image` base64; (c) erro determinístico com `component_id` inexistente (`isError: true`). Verificar execução de `OpenAudioStudio --smoke-mcp` com código de saída `0`.
- [x] 3.2 **[Docs / Gates]** Documentar os parâmetros, payload de saída e exemplo JSON-RPC de `studio_ui_screenshot` em `docs/specs/mcp-protocol.md` (incluindo nota de transição/ponteiro para `openspec/specs/mcp-automation/spec.md`) e validar a suíte completa executando `powershell -NoProfile -File scripts/verify.ps1 -Layer studio` e `powershell -NoProfile -File scripts/verify.ps1 -Layer docs` com `RESULT: OK`.
