# Contrato do Protocolo MCP — OpenAudio Studio

> **Nota de transição OpenSpec**: Os requisitos normativos da capacidade `mcp-automation` estão definidos na especificação canônica [openspec/specs/mcp-automation/spec.md](../../openspec/specs/mcp-automation/spec.md). Para configuração de clientes, modos de transporte (`stdio` e TCP `127.0.0.1:7451`), tabela de `ComponentID`s e demais ferramentas do servidor, consulte o [Guia do Servidor MCP](mcp-server-guide.md).

---

## Ferramenta: `studio_ui_screenshot`

Renderiza em memória, na JUCE Message Thread (`run_on_message_thread`), um snapshot bitmap (`juce::Image`) da janela raiz do OpenAudio Studio (`root_component`) ou de um subcomponente específico identificado por `component_id` usando `juce::Component::createComponentSnapshot` e codifica o resultado em formato PNG (`juce::PNGImageFormat`).

Funciona tanto em execução GUI desktop (`MainComponent`) quanto em modo headless (`HeadlessUiHost`, usado por `--smoke-mcp`), sem depender de handles nativos de janela do sistema operacional (`HWND`) ou APIs de captura de desktop.

### Pré-requisitos e Execução

- Servidor MCP em execução (`OpenAudioStudio --mcp` ou porta TCP configurada via `--mcp-port=<porta>`).
- `ctx_.root_component` inicializado com dimensões válidas (`width > 0` e `height > 0`).
- **Limitações**:
  - Não realiza gravação contínua de vídeo nem streaming de múltiplos frames.
  - Captura apenas a hierarquia JUCE sob `ctx_.root_component` (não captura janelas externas do sistema operacional nem pipelines OpenGL fora de `Component::paint`).

### Parâmetros (`arguments`)

Todos os parâmetros são opcionais:

| Parâmetro | Tipo | Padrão | Descrição |
| :--- | :--- | :--- | :--- |
| `component_id` | `string` | `""` (vazio) | `ComponentID` semântico do componente alvo (ex.: `"timeline_view"`, `"deck_spatializer"`, `"deck_effects"`). Quando omitido ou vazio, captura `root_component`. |
| `output_path` | `string` | omitido | Caminho de arquivo `.png` onde a imagem será gravada em disco. Diretórios pais inexistentes são criados automaticamente. |
| `scale` | `number` | `1.0` | Fator de escala da renderização. Aplicado *clamp* determinístico no intervalo `[0.25, 2.0]`. |
| `include_base64` | `boolean` | `true` se `output_path` omitido; `false` se `output_path` informado | Controla se o bloco MCP `{ "type": "image", "mimeType": "image/png", "data": "<base64>" }` é incluído em `result.content`. Pode ser sobrescrito explicitamente. |

### Payload de Saída (`result`)

Em caso de sucesso (`isError: false`), `result.content` contém:

1. **Primeiro item (`type: "text"`)**: string JSON serializada com os metadados da captura:
   - `component_id` (`string`): ID informado ou `"root"` quando capturada a janela completa.
   - `width` (`integer`): largura em pixels da imagem PNG renderizada.
   - `height` (`integer`): altura em pixels da imagem PNG renderizada.
   - `scale` (`number`): fator de escala efetivo após clamp em `[0.25, 2.0]`.
   - `byte_size` (`integer`): tamanho em bytes do payload PNG gerado.
   - `output_path` (`string`, presente apenas quando `output_path` foi fornecido): caminho absoluto resolvido do arquivo gravado em disco.
2. **Segundo item (`type: "image"`, presente quando `include_base64` é efetivamente `true`)**:
   - `type`: `"image"`
   - `mimeType`: `"image/png"`
   - `data`: string PNG codificada em Base64 (cabeçalho binário `89 50 4E 47`).
3. **`diff`**: array RFC 6902 JSON Patch (vazio, pois a captura não altera o `AuthoringModel`).

### Erros Determinísticos (`isError: true`)

- **Sem componente raiz**: retorna `isError: true` quando `ctx_.root_component == nullptr`.
- **`component_id` inexistente**: retorna `isError: true` com mensagem indicando o `component_id` não encontrado, sem criar arquivo em disco.
- **Área não renderizável**: retorna `isError: true` quando `width <= 0 || height <= 0` no componente alvo.
- **Falha de E/S em `output_path`**: retorna `isError: true` caso o caminho seja inválido, não seja possível criar diretórios pais ou falhe a abertura/gravação do arquivo PNG.

---

### Exemplos JSON-RPC 2.0

#### Exemplo 1: Captura da janela completa para arquivo em disco (`output_path`)

**Requisição:**
```json
{
  "jsonrpc": "2.0",
  "id": 101,
  "method": "tools/call",
  "params": {
    "name": "studio_ui_screenshot",
    "arguments": {
      "output_path": "C:/temp/studio_overview.png",
      "scale": 1.0
    }
  }
}
```

**Resposta:**
```json
{
  "jsonrpc": "2.0",
  "id": 101,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "{\"byte_size\":48210,\"component_id\":\"root\",\"height\":800,\"output_path\":\"C:\\\\temp\\\\studio_overview.png\",\"scale\":1.0,\"width\":1280}"
      }
    ],
    "diff": [],
    "isError": false
  }
}
```

#### Exemplo 2: Captura escopada por `component_id` com retorno Base64 inline

**Requisição:**
```json
{
  "jsonrpc": "2.0",
  "id": 102,
  "method": "tools/call",
  "params": {
    "name": "studio_ui_screenshot",
    "arguments": {
      "component_id": "timeline_view",
      "scale": 0.5
    }
  }
}
```

**Resposta:**
```json
{
  "jsonrpc": "2.0",
  "id": 102,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "{\"byte_size\":9412,\"component_id\":\"timeline_view\",\"height\":272,\"scale\":0.5,\"width\":500}"
      },
      {
        "type": "image",
        "mimeType": "image/png",
        "data": "iVBORw0KGgoAAAANSUhEUg..."
      }
    ],
    "diff": [],
    "isError": false
  }
}
```

#### Exemplo 3: Erro determinístico com `component_id` inexistente

**Requisição:**
```json
{
  "jsonrpc": "2.0",
  "id": 103,
  "method": "tools/call",
  "params": {
    "name": "studio_ui_screenshot",
    "arguments": {
      "component_id": "inexistente_id"
    }
  }
}
```

**Resposta:**
```json
{
  "jsonrpc": "2.0",
  "id": 103,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Component not found: inexistente_id"
      }
    ],
    "diff": [],
    "isError": true
  }
}
```

---

## Ferramenta: `studio_ui_highlight`

Ativa, atualiza ou limpa programaticamente, na JUCE Message Thread (`run_on_message_thread`), o overlay não-interativo `TutorialHighlightOverlay` sobre o `MainComponent` (ou `HeadlessUiHost`) para aplicar efeito *spotlight* (escurecimento periférico em quatro quadrantes, contorno arredondado com tokens de `OaeTheme::Colors` e pílula opcional de legenda sem recorte) sobre o componente identificado por `component_id`.

Por ser um filho direto no topo da ordem Z que retorna `false` em `hitTest` e não intercepta cliques nem foco de teclado, eventos de mouse/teclado reais e chamadas MCP (`studio_ui_mouse`, `studio_ui_set_control`, `studio_ui_key`) atravessam o overlay diretamente até o controle subjacente, enquanto capturas de janela inteira via `studio_ui_screenshot` registram o destaque ativo.

### Pré-requisitos e Execução

- Servidor MCP em execução (`OpenAudioStudio --mcp` ou porta TCP configurada via `--mcp-port=<porta>`).
- `ctx_.root_component` e `ctx_.highlight_overlay` inicializados e visíveis para `component_id` alvo (quando `clear` for falso).

### Parâmetros (`arguments`)

| Parâmetro | Tipo | Padrão | Descrição |
| :--- | :--- | :--- | :--- |
| `component_id` | `string` | obrigatório se `clear` falso | `ComponentID` semântico visível e layoutado a destacar (ex.: `"timeline_view"`, `"layers.card.1.mask_canvas"`, `"deck_mixer"`). |
| `label` | `string` | `""` (vazio) | Legenda curta opcional exibida em pílula adjacente ao contorno sem ser cortada nas bordas da janela. |
| `dim_opacity` | `number` | `0.45` | Opacidade do escurecimento periférico ao redor do controle alvo, limitada ao intervalo `[0.0, 0.85]`. |
| `color` | `string` | `"cyan"` | Cor de destaque mapeada para `OaeTheme::Colors`: `"cyan"` (`accentCyan`), `"amber"` (`accentAmber`) ou `"green"` (`accentGreen`). |
| `clear` | `boolean` | `false` | Quando `true`, desativa e oculta o overlay de destaque e ignora `component_id`. |

### Payload de Saída (`result`)

Em caso de sucesso (`isError: false`), `result.content[0]` (`type: "text"`) contém JSON serializado:

- **Ativação (`clear: false`)**: `{ "active": true, "component_id": "<id>", "bounds": { "x": ..., "y": ..., "w": ..., "h": ... }, "label": "...", "dim_opacity": 0.45, "color": "cyan" }`.
- **Limpeza (`clear: true`)**: `{ "active": false }`.
- **`diff`**: array RFC 6902 vazio.

### Erros Determinísticos (`isError: true`)

- **Sem raiz ou overlay**: retorna `isError: true` se `ctx_.root_component` ou `ctx_.highlight_overlay` for nulo.
- **`component_id` ausente ou inexistente**: retorna `isError: true` quando `clear` é falso e `component_id` está vazio/omitido ou não existe na árvore.
- **Alvo oculto ou sem área**: retorna `isError: true` se o componente alvo não estiver visível sob `root_component` ou possuir `width <= 0 || height <= 0`.
- **Parâmetro inválido**: retorna `isError: true` se `color` não estiver em `["cyan", "amber", "green"]` ou se `dim_opacity` não for numérico finito; erros preservam o estado anterior do overlay.

### Exemplos JSON-RPC 2.0

#### Exemplo 1: Ativação de destaque com legenda e cor âmbar

**Requisição:**
```json
{
  "jsonrpc": "2.0",
  "id": 104,
  "method": "tools/call",
  "params": {
    "name": "studio_ui_highlight",
    "arguments": {
      "component_id": "deck_spatializer",
      "label": "Cone 3D e Rolloff",
      "dim_opacity": 0.5,
      "color": "amber"
    }
  }
}
```

**Resposta:**
```json
{
  "jsonrpc": "2.0",
  "id": 104,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "{\"active\":true,\"bounds\":{\"h\":230,\"w\":1280,\"x\":0,\"y\":570},\"color\":\"amber\",\"component_id\":\"deck_spatializer\",\"dim_opacity\":0.5,\"label\":\"Cone 3D e Rolloff\"}"
      }
    ],
    "diff": [],
    "isError": false
  }
}
```

#### Exemplo 2: Limpeza explícita do destaque

**Requisição:**
```json
{
  "jsonrpc": "2.0",
  "id": 105,
  "method": "tools/call",
  "params": {
    "name": "studio_ui_highlight",
    "arguments": {
      "clear": true
    }
  }
}
```

**Resposta:**
```json
{
  "jsonrpc": "2.0",
  "id": 105,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "{\"active\":false}"
      }
    ],
    "diff": [],
    "isError": false
  }
}
```
