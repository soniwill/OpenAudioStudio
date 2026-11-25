# Proposal: Ferramenta MCP `studio_ui_screenshot` para captura visual da interface JUCE

## Why

Agentes operando em sessões de shell em segundo plano no Windows não conseguem capturar janelas do desktop do sistema operacional via GDI (`Graphics.CopyFromScreen` falha com `Win32Exception: Identificador inválido`), e os hosts headless da suíte de smoke tests (`HeadlessUiHost`) sequer criam uma janela nativa do SO. Embora o servidor MCP `openaudio-studio` já ofereça inspeção semântica da árvore de componentes (`studio_ui_inspect`), ele ainda não fornece imagens bitmap renderizadas (`PNG`) da interface JUCE para validação visual direta no chat ou regressão visual automatizada.

## What Changes

- Adicionar a nova ferramenta `studio_ui_screenshot` no catálogo e no despachante JSON-RPC do servidor MCP do OpenAudio Studio (`studio/Source/MCP/McpProtocol.h`, `studio/Source/MCP/McpServer.h`, `studio/Source/MCP/McpServer.cpp`).
- Renderizar na JUCE Message Thread (`run_on_message_thread`) um snapshot bitmap da janela raiz (`ctx_.root_component`) ou de um subcomponente específico localizado recursivamente por `component_id` via `juce::Component::createComponentSnapshot(target->getLocalBounds(), true, scale)` e codificá-lo em PNG via `juce::PNGImageFormat`.
- Suportar persistência direta em disco via parâmetro opcional `output_path` (criando diretórios pais quando necessário), fator de escala configurável (`scale` limitado a `[0.25, 2.0]`, padrão `1.0`) e controle explícito de retorno base64 (`include_base64`) emitindo bloco MCP `{ "type": "image", "mimeType": "image/png", "data": "<base64>" }` além de um bloco `text` com metadados JSON (`component_id`, `width`, `height`, `scale`, `output_path`, `byte_size`).
- Estender `run_smoke_mcp()` em `studio/Source/Main.cpp` para validar a captura da janela completa, a captura escopada por `component_id` (ex.: `timeline_view`) com verificação do cabeçalho PNG (`89 50 4E 47`) e o erro determinístico para `component_id` inexistente ou componente sem área visível (`width <= 0 || height <= 0`).
- **Exclusões**: Gravação contínua de vídeo/stream de frames, captura de janelas externas do desktop fora da hierarquia JUCE e readback direto de buffers OpenGL fora do fluxo padrão `Component::paint` do JUCE.

## Capabilities

### New Capabilities

- `mcp-automation`: Automação, inspeção semântica e captura visual determinística do OpenAudio Studio via Model Context Protocol (MCP) sobre stdio e TCP, iniciando com o contrato de captura bitmap PNG `studio_ui_screenshot`.

### Modified Capabilities

Nenhuma capacidade canônica existente tem seus requisitos alterados por esta change.

## Impact

- **Studio MCP (`studio/Source/MCP/`)**: `McpProtocol.h` (`mcp::get_tools_catalog()`), `McpServer.h` e `McpServer.cpp` (`handle_tools_call`, `tool_ui_screenshot`).
- **Dev Harness / Smoke (`studio/Source/`)**: `Main.cpp` (`run_smoke_mcp()`).
- **Documentação (`docs/specs/`)**: `docs/specs/mcp-protocol.md` atualizado com registro da ferramenta e ponteiro para `openspec/specs/mcp-automation/spec.md` após a migração/sincronização.
- **Engine / C ABI / Unity**: Sem impacto; mudança restrita à camada de automação MCP no Studio.
