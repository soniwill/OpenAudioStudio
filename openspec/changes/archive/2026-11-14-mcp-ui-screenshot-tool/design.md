# Design: Ferramenta MCP `studio_ui_screenshot`

## Context

Veja `proposal.md` para a motivação completa. Atualmente, todas as ferramentas de UI do `McpServer` (`studio_trigger_control`, `studio_ui_inspect`, `studio_ui_key`, `studio_ui_type`, `studio_ui_mouse`, `studio_ui_set_control`) já são despachadas na JUCE Message Thread através de `McpServer::run_on_message_thread` em [McpServer.cpp](../../../studio/Source/MCP/McpServer.cpp). Como o servidor já mantém `ctx_.root_component` apontando para `MainComponent` (em modo GUI desktop) ou `HeadlessUiHost` (em `--smoke-mcp` e `--headless`), a captura diretamente sobre a árvore de componentes JUCE evita dependência de handles de janela do SO ou captura GDI de desktop.

## Goals / Non-Goals

**Goals:**
- Renderizar snapshots bitmap (`juce::Image`) em memória de `ctx_.root_component` ou de qualquer subcomponente encontrado por `find_component_recursive(ctx_.root_component, cid)` usando `createComponentSnapshot`.
- Codificar o bitmap em PNG (`juce::PNGImageFormat`) via `juce::MemoryOutputStream`, permitindo escrita em disco (`output_path`) e/ou retorno de bloco MCP `image` em base64 (`include_base64`).
- Aplicar clamp determinístico no fator de escala `scale` no intervalo `[0.25, 2.0]` (padrão `1.0`).
- Validar o comportamento determinístico em `run_smoke_mcp()` (`studio/Source/Main.cpp`) tanto no host headless quanto no servidor desktop real.

**Non-Goals:**
- Gravação contínua de vídeo ou streaming de múltiplos frames por segundo.
- Captura de outras janelas do sistema operacional ou monitores externos fora de `ctx_.root_component`.
- Readback direto de pipelines OpenGL customizados fora da pintura padrão `Component::paint` do JUCE.

## Decisions

1. **Mecanismo de renderização in-process via JUCE**:
   - **Escolha**: Executar `target->createComponentSnapshot(target->getLocalBounds(), true, scale)` dentro de `run_on_message_thread` e codificar via `juce::PNGImageFormat::writeImageToStream`.
   - **Alternativa considerada**: Captura via API Win32 (`PrintWindow` / `BitBlt` / `Graphics.CopyFromScreen`). Rejeitada porque falha em sessões em segundo plano sem estação desktop interativa (`Win32Exception: Identificador inválido`) e não funciona em `HeadlessUiHost`, que não possui HWND nativo.
2. **Controle inteligente de payload (`output_path` vs `include_base64`)**:
   - **Escolha**: Quando `output_path` for informado, `include_base64` assume padrão `false` (podendo ser forçado para `true`); quando `output_path` for omitido, `include_base64` assume padrão `true`.
   - **Racional**: PNGs de janela inteira (`1280x800`+) codificados em base64 geram 80–250 KB por resposta JSON-RPC. Clientes locais com acesso ao sistema de arquivos (como agentes IDE) podem ler o PNG diretamente via arquivo sem inflar logs ou contexto JSON-RPC, enquanto clientes MCP remotos/TCP recebem o bloco padrão `{ "type": "image", "mimeType": "image/png", "data": "<base64>" }` automaticamente quando omitem `output_path`.
3. **Contrato de esquema em `mcp::get_tools_catalog()` (`studio/Source/MCP/McpProtocol.h`)**:
   - `component_id` (`string`, opcional): ID semântico do componente alvo; vazio ou omitido captura `root_component`.
   - `output_path` (`string`, opcional): Caminho do arquivo `.png` de destino em disco.
   - `scale` (`number`, opcional): Fator de escala limitado a `[0.25, 2.0]` (padrão `1.0`).
   - `include_base64` (`boolean`, opcional): Controla inclusão explícita do bloco `image` base64 no array `content`.
4. **Formato de retorno MCP**:
   - Primeiro bloco `content`: `{ "type": "text", "text": "<json>" }` contendo `component_id` (ou `"root"`), `width`, `height`, `scale`, `byte_size` e `output_path` (quando gravado em disco).
   - Segundo bloco `content` (quando `include_base64 == true`): `{ "type": "image", "mimeType": "image/png", "data": "<base64>" }`.

## Risks / Trade-offs

- **Componentes ocultos ou sem bounds calculados (`width <= 0 || height <= 0`)** → Validar dimensões locais do componente alvo antes de invocar `createComponentSnapshot` e retornar `isError: true` com mensagem diagnóstica clara em vez de imagem `1x1` vazia ou falha silenciosa.
- **Latência na Message Thread e tamanho de resposta TCP/stdio** → A renderização `1280x800` em CPU mais compressão PNG leva poucos milissegundos e o padrão `include_base64 = false` quando `output_path` está presente evita saturação do buffer de resposta.
- **Falha de E/S em `output_path` inválido ou sem permissão** → Criar diretórios pais com tratamento explícito de erro e retornar `isError: true` caso o stream de saída não consiga gravar o arquivo PNG.
