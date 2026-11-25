# OpenAudio Studio — Guia de Uso do Servidor MCP (Model Context Protocol)

Este documento descreve como configurar, conectar e operar o **Servidor MCP Local (JSON-RPC 2.0)** do **OpenAudio Studio**, implementado como parte do **Épico 6 (`MCP-01` a `MCP-05`)**.

O servidor permite que agentes autônomos de Inteligência Artificial (como Claude Desktop, Antigravity, Cursor ou scripts customizados em Python/Node.js) inspecionem e controlem diretamente a DAW OpenAudio Studio em tempo real, executando ações de autoria, acionamento de controles visuais, transporte e compilação de bancos sonoros com diffing atômico via **RFC 6902 JSON Patch**.

---

## 1. Modos de Operação

O executável da DAW (`OpenAudioStudio.exe`) expõe dois modos de conectividade MCP simultâneos:

| Modo | Transporte | Endereço / Canal | Cenário de Uso |
| :--- | :--- | :--- | :--- |
| **Stdio** | Entrada e Saída Padrão (`stdin`/`stdout`) | Processo filho | Padrão da especificação MCP para clientes locais como Claude Desktop, Antigravity e Cursor. |
| **TCP Socket** | Socket IPv4 Local | `127.0.0.1:7451` | Integrações em rede local, scripts externos (Python, Node.js, C#), testes de integração e automações CLI. |

### Inicialização via Linha de Comando

Para iniciar o OpenAudio Studio operando como servidor MCP:

```powershell
# Execução direta com flag --mcp (inicia stdio e escuta na porta 7451)
.\build\Studio_Release\OpenAudioStudio_artefacts\Release\OpenAudioStudio.exe --mcp
```

*(O servidor mantém a engine de áudio inicializada, o documento de projeto e a árvore de UI pronta para comandos com despacho thread-safe para a JUCE Message Thread).*

---

## 2. Configuração em Clientes MCP

### Claude Desktop

Adicione a entrada correspondente no arquivo de configuração do Claude Desktop (`%APPDATA%\Claude\claude_desktop_config.json` no Windows):

```json
{
  "mcpServers": {
    "openaudio-studio": {
      "command": "C:\\caminho\\OpenAudioStudio\\build\\Studio_Bundle_Release\\OpenAudio Studio.exe",
      "args": ["--mcp"]
    }
  }
}
```

### Antigravity / Cursor

No arquivo de configuração MCP correspondente (`~/.gemini/config/mcp_config.json` no Antigravity):

#### Modo Não-Intrusivo (Recomendado via Bridge Node.js)
Conecta ao Studio apenas quando o usuário já abriu o programa manualmente, sem forçar abertura automática de janela caso esteja fechado:

```json
{
  "mcpServers": {
    "openaudio-studio": {
      "command": "node",
      "args": ["C:\\caminho\\OpenAudioStudio\\scripts\\mcp_bridge.js"]
    }
  }
}
```

#### Modo Direto (Inicialização Forçada)
Inicia o executável do Studio diretamente sempre que o Antigravity carrega os servidores MCP:

```json
{
  "mcpServers": {
    "openaudio-studio": {
      "command": "C:\\caminho\\OpenAudioStudio\\build\\Studio_Bundle_Release\\OpenAudio Studio.exe",
      "args": ["--mcp"]
    }
  }
}
```

### Conexão via Socket TCP (Python / Scripts)

O servidor aceita conexões socket TCP raw com mensagens JSON delimitadas por quebra de linha (`\n`):

```python
import socket
import json

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect(("127.0.0.1", 7451))

# Inicialização MCP
request = {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "initialize",
    "params": {"protocolVersion": "2024-11-05"}
}
client.sendall((json.dumps(request) + "\n").encode("utf-8"))

response = client.recv(4096)
print(json.loads(response.decode("utf-8")))
client.close()
```

---

## 3. Ciclo de Vida do Protocolo JSON-RPC 2.0

O servidor adere estritamente à especificação **JSON-RPC 2.0** e suporta os seguintes métodos padrão:

### 3.1 `initialize`
Negociação inicial de capacidades e versão de protocolo.

**Requisição:**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "protocolVersion": "2024-11-05"
  }
}
```

**Resposta:**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "protocolVersion": "2024-11-05",
    "capabilities": {
      "tools": {},
      "resources": {}
    },
    "serverInfo": {
      "name": "openaudio-studio-mcp",
      "version": "0.1.0"
    }
  }
}
```

### 3.2 Códigos de Erro JSON-RPC
- `-32700`: Erro de parse JSON.
- `-32600`: Requisição JSON-RPC inválida.
- `-32601`: Método não encontrado.
- `-32602`: Parâmetros inválidos para a tool ou resource.
- `-32603`: Erro interno do servidor MCP.
- `-32000`: Falha de execução da ação na DAW.

---

## 4. Catálogo de Ferramentas (MCP Tools)

Liste todas as ferramentas disponíveis através do método `tools/list`:

```json
{ "jsonrpc": "2.0", "id": 2, "method": "tools/list" }
```

### 4.1 `studio_trigger_control`
Aciona programaticamente qualquer controle visual da DAW JUCE utilizando seu `ComponentID` semântico determinístico.

- **Parâmetros:**
  - `component_id` (string, obrigatório): Identificador do controle (ex.: `"btn_play"`, `"btn_build_bank"`).
- **Exemplo de Chamada:**
  ```json
  {
    "jsonrpc": "2.0",
    "id": 3,
    "method": "tools/call",
    "params": {
      "name": "studio_trigger_control",
      "arguments": {
        "component_id": "btn_play"
      }
    }
  }
  ```
- **Retorno:**
  ```json
  {
    "jsonrpc": "2.0",
    "id": 3,
    "result": {
      "status": "triggered",
      "component_id": "btn_play"
    }
  }
  ```

### 4.2 `studio_select_item`
Altera o foco de seleção do `AuthoringModel` da DAW, sincronizando a UI (Inspector, Mixer e Contextual Deck).

- **Parâmetros:**
  - `type` (string, obrigatório): `"event"` | `"bus"` | `"parameter"` | `"snapshot"`.
  - `path` (string, obrigatório): Caminho canônico do item (ex.: `"event:/sfx/coin"`, `"bus:/music"`).
- **Exemplo de Chamada:**
  ```json
  {
    "jsonrpc": "2.0",
    "id": 4,
    "method": "tools/call",
    "params": {
      "name": "studio_select_item",
      "arguments": {
        "type": "event",
        "path": "event:/sfx/coin"
      }
    }
  }
  ```
- **Retorno:**
  ```json
  {
    "jsonrpc": "2.0",
    "id": 4,
    "result": {
      "status": "selected",
      "type": "event",
      "path": "event:/sfx/coin"
    }
  }
  ```

### 4.3 `studio_set_mixer_fader`
Ajusta o fader de volume de um barramento (Master, Bus ou VCA) em decibéis. O servidor automaticamente converte o valor em dB para ganho linear no modelo canônico do projeto.

- **Parâmetros:**
  - `bus_path` (string, obrigatório): `"bus:/"` para Master ou o caminho do bus/vca (ex.: `"bus:/sfx"`).
  - `volume_db` (number, obrigatório): Volume em dB (intervalo típico `-80.0` a `+10.0`).
- **Exemplo de Chamada:**
  ```json
  {
    "jsonrpc": "2.0",
    "id": 5,
    "method": "tools/call",
    "params": {
      "name": "studio_set_mixer_fader",
      "arguments": {
        "bus_path": "bus:/",
        "volume_db": -6.0
      }
    }
  }
  ```
- **Retorno (com Patch RFC 6902):**
  ```json
  {
    "jsonrpc": "2.0",
    "id": 5,
    "result": {
      "status": "updated",
      "bus_path": "bus:/",
      "volume_db": -6.0,
      "patch": [
        {
          "op": "replace",
          "path": "/buses/0/volume",
          "value": 0.5011872336
        }
      ]
    }
  }
  ```

### 4.4 `studio_move_clip`
Move uma região/instrumento de áudio na timeline multi-track para uma nova faixa ou novo instante de tempo em milissegundos.

- **Parâmetros:**
  - `event_path` (string, obrigatório): Identificador do evento pai.
  - `track_index` (integer, obrigatório): Índice zero-based da trilha de destino.
  - `instrument_index` (integer, obrigatório): Índice zero-based do instrumento na trilha.
  - `new_offset_ms` (number, obrigatório): Novo tempo de início em milissegundos.
- **Exemplo de Chamada:**
  ```json
  {
    "jsonrpc": "2.0",
    "id": 6,
    "method": "tools/call",
    "params": {
      "name": "studio_move_clip",
      "arguments": {
        "event_path": "event:/sfx/coin",
        "track_index": 0,
        "instrument_index": 0,
        "new_offset_ms": 250.0
      }
    }
  }
  ```
- **Retorno (com Patch RFC 6902):**
  ```json
  {
    "jsonrpc": "2.0",
    "id": 6,
    "result": {
      "status": "moved",
      "event_path": "event:/sfx/coin",
      "track_index": 0,
      "instrument_index": 0,
      "new_offset_ms": 250.0,
      "patch": [
        {
          "op": "replace",
          "path": "/events/0/tracks/0/instruments/0/offset_ms",
          "value": 250.0
        }
      ]
    }
  }
  ```

### 4.5 `studio_transport`
Controla o motor de audição em tempo real da DAW.

- **Parâmetros:**
  - `action` (string, obrigatório): `"play"` | `"pause"` | `"resume"` | `"stop"` | `"seek"`.
  - `position_ms` (number, opcional): Posição alvo para seek em milissegundos.
- **Exemplo de Chamada:**
  ```json
  {
    "jsonrpc": "2.0",
    "id": 7,
    "method": "tools/call",
    "params": {
      "name": "studio_transport",
      "arguments": {
        "action": "play"
      }
    }
  }
  ```

### 4.6 `studio_build_bank`
Dispara a compilação atômica do banco e catálogo via `AsyncBankPublisher`.

- **Parâmetros:**
  - `destination_path` (string, opcional): Caminho de saída `.bank`. Se omitido, compila na pasta de staging temporária de preview.
- **Exemplo de Chamada:**
  ```json
  {
    "jsonrpc": "2.0",
    "id": 8,
    "method": "tools/call",
    "params": {
      "name": "studio_build_bank",
      "arguments": {
        "destination_path": "build/banks/master.bank"
      }
    }
  }
  ```
- **Retorno:**
  ```json
  {
    "jsonrpc": "2.0",
    "id": 8,
    "result": {
      "status": "success",
      "output_bank": "build/banks/master.bank",
      "catalog_path": "build/banks/master.bank.catalog.json"
    }
  }
  ```

### 4.7 `studio_ui_screenshot`
Captura um snapshot bitmap PNG da janela completa (`root_component`) ou de um subcomponente específico identificado por `component_id` na JUCE Message Thread (`juce::Component::createComponentSnapshot` + `juce::PNGImageFormat`). Consulte o contrato detalhado e exemplos em [mcp-protocol.md](mcp-protocol.md) e na especificação canônica da capacidade `mcp-automation` em [openspec/specs/mcp-automation/spec.md](../../openspec/specs/mcp-automation/spec.md).

- **Parâmetros (todos opcionais):**
  - `component_id` (string): ID semântico do componente (ex.: `"timeline_view"`). Vazio ou omitido captura a janela inteira.
  - `output_path` (string): caminho `.png` de saída em disco (cria diretórios pais automaticamente).
  - `scale` (number): fator de escala limitado a `[0.25, 2.0]` (padrão `1.0`).
  - `include_base64` (boolean): padrão `true` se `output_path` omitido, `false` se `output_path` fornecido.
- **Exemplo de Chamada:**
  ```json
  {
    "jsonrpc": "2.0",
    "id": 9,
    "method": "tools/call",
    "params": {
      "name": "studio_ui_screenshot",
      "arguments": {
        "component_id": "timeline_view"
      }
    }
  }
  ```

### 4.8 `studio_ui_highlight`
Ativa, atualiza ou limpa o overlay visual não-interativo (`TutorialHighlightOverlay`) sobre o controle alvo na JUCE Message Thread, desenhando máscara de escurecimento periférica, contorno nos tokens de `OaeTheme` (`cyan`, `amber` ou `green`) e pílula de legenda opcional sem interferir em cliques ou foco. Consulte o contrato completo e erros determinísticos em [mcp-protocol.md](mcp-protocol.md) e na especificação canônica de `mcp-automation` em [openspec/specs/mcp-automation/spec.md](../../openspec/specs/mcp-automation/spec.md).

- **Parâmetros:**
  - `component_id` (string): ID semântico do componente alvo (obrigatório quando `clear` é `false`).
  - `label` (string, opcional): legenda curta exibida próxima ao destaque.
  - `dim_opacity` (number, opcional, padrão `0.45`): opacidade do escurecimento em `[0.0, 0.85]`.
  - `color` (string, opcional, padrão `"cyan"`): `"cyan"`, `"amber"` ou `"green"`.
  - `clear` (boolean, opcional, padrão `false`): quando `true`, desativa e oculta o overlay.
- **Exemplo de Chamada:**
  ```json
  {
    "jsonrpc": "2.0",
    "id": 10,
    "method": "tools/call",
    "params": {
      "name": "studio_ui_highlight",
      "arguments": {
        "component_id": "timeline_view",
        "label": "Timeline (Exploration & Combat)",
        "dim_opacity": 0.45,
        "color": "cyan"
      }
    }
  }
  ```

---

## 5. Catálogo de Recursos (MCP Resources)

Liste os recursos através do método `resources/list`:

```json
{ "jsonrpc": "2.0", "id": 9, "method": "resources/list" }
```

Para ler o conteúdo de um recurso, envie `resources/read`:

### 5.1 `oae://project/state`
Retorna o documento JSON de autoria atual completo da DAW.

**Requisição:**
```json
{
  "jsonrpc": "2.0",
  "id": 10,
  "method": "resources/read",
  "params": { "uri": "oae://project/state" }
}
```
**Resposta:**
```json
{
  "jsonrpc": "2.0",
  "id": 10,
  "result": {
    "contents": [
      {
        "uri": "oae://project/state",
        "mimeType": "application/json",
        "text": "{\n  \"schema_version\": 1,\n  \"name\": \"GameAudio\",\n  \"events\": [...],\n  \"buses\": [...]\n}"
      }
    ]
  }
}
```

### 5.2 `oae://audio/meters`
Retorna telemetria acústica instantânea em dB (RMS e Peak) para o Master e todos os barramentos ativos.

**Requisição:**
```json
{
  "jsonrpc": "2.0",
  "id": 11,
  "method": "resources/read",
  "params": { "uri": "oae://audio/meters" }
}
```
**Resposta:**
```json
{
  "jsonrpc": "2.0",
  "id": 11,
  "result": {
    "contents": [
      {
        "uri": "oae://audio/meters",
        "mimeType": "application/json",
        "text": "{\n  \"master\": { \"peak_db\": -3.2, \"rms_db\": -14.1 },\n  \"buses\": [\n    { \"name\": \"sfx\", \"peak_db\": -4.5, \"rms_db\": -16.0 },\n    { \"name\": \"music\", \"peak_db\": -12.0, \"rms_db\": -18.4 }\n  ]\n}"
      }
    ]
  }
}
```

### 5.3 `oae://diagnostics/log`
Retorna as entradas de console e log de diagnóstico da DAW.

**Requisição:**
```json
{
  "jsonrpc": "2.0",
  "id": 12,
  "method": "resources/read",
  "params": { "uri": "oae://diagnostics/log" }
}
```
**Resposta:**
```json
{
  "jsonrpc": "2.0",
  "id": 12,
  "result": {
    "contents": [
      {
        "uri": "oae://diagnostics/log",
        "mimeType": "text/plain",
        "text": "[13:30:00] Engine initialized successfully.\n[13:30:01] Project loaded: project.json\n[13:30:02] Bank compiled: ExampleGameProject.bank (2173264 bytes)\n"
      }
    ]
  }
}
```

---

## 6. Tabela de ComponentIDs Semânticos (Automação de UI)

A ferramenta `studio_trigger_control` aceita os seguintes `ComponentID`s determinísticos registrados nos componentes da DAW:

| Painel | ComponentID | Tipo de Controle | Ação Realizada |
| :--- | :--- | :--- | :--- |
| **Transport Bar** | `btn_new_project` | `juce::TextButton` | Cria novo projeto em branco |
| **Transport Bar** | `btn_open_project` | `juce::TextButton` | Abre caixa de diálogo de projeto |
| **Transport Bar** | `btn_save_project` | `juce::TextButton` | Salva o projeto atual no disco |
| **Transport Bar** | `btn_build_bank` | `juce::TextButton` | Aciona compilação e publicação de bancos |
| **Transport Bar** | `btn_play` | `juce::TextButton` | Inicia reprodução do evento selecionado |
| **Transport Bar** | `btn_pause` | `juce::TextButton` | Pausa a reprodução mantendo a agulha |
| **Transport Bar** | `btn_stop` | `juce::TextButton` | Interrompe reprodução e reseta playhead |
| **Transport Bar** | `btn_loop` | `juce::TextButton` | Alterna modo de repetição contínua |
| **Transport Bar** | `btn_key_off` | `juce::TextButton` | Emite comando KeyOff para sustentações |
| **Mixer Deck** | `mixer_fader_master` | `juce::Slider` | Fader do canal Master |
| **Mixer Deck** | `mixer_mute_master` | `juce::TextButton` | Mute do canal Master |
| **Mixer Deck** | `mixer_solo_master` | `juce::TextButton` | Solo do canal Master |
| **Mixer Deck** | `mixer_fader_bus_<nome>` | `juce::Slider` | Fader do barramento especificado |
| **Mixer Deck** | `mixer_mute_bus_<nome>` | `juce::TextButton` | Mute do barramento especificado |
| **Mixer Deck** | `mixer_solo_bus_<nome>` | `juce::TextButton` | Solo do barramento especificado |
| **Mixer Deck** | `mixer_fader_vca_<nome>` | `juce::Slider` | Fader do VCA especificado |
| **Mixer Deck** | `mixer_mute_vca_<nome>` | `juce::TextButton` | Mute do VCA especificado |
| **Timeline** | `timeline_view` | `juce::Component` | Área de visualização das trilhas e réguas |
| **Timeline** | `minimap_view` | `juce::Component` | Visão panorâmica geral (bird's-eye minimap) |
| **Timeline** | `track_lane_<index>` | `juce::Component` | Faixa da trilha no índice especificado |
| **Timeline** | `track_color_swatch_<index>` | `juce::TextButton` | Seletor de cor da faixa no índice `<index>` |
| **Timeline** | `track_title_<index>` | `juce::Label` | Rótulo editável com o nome da faixa `<index>` |
| **Timeline** | `track_volume_<index>` | `OaeSlider` (`juce::Slider`) | Slider de volume da faixa `<index>` em dB (`-80.0..10.0`), aplicado ao vivo na audição |
| **Timeline** | `track_pan_<index>` | `OaeSlider` (`juce::Slider`) | Slider de pan estéreo da faixa `<index>` (`-1.0..1.0`), aplicado ao vivo na audição |
| **Timeline** | `track_mute_<index>` | `juce::TextButton` | Botão `M` (Mute exclusivo de audição) com `toggle_state` sincronizado ao modelo |
| **Timeline** | `track_solo_<index>` | `juce::TextButton` | Botão `S` (Solo exclusivo de audição) com `toggle_state` sincronizado ao modelo |
| **Timeline** | `track_auto_<index>` | `juce::TextButton` | Botão `A` (expande/recolhe automação da faixa) com `toggle_state` sincronizado |
| **Bottom Deck** | `deck_tab_bar` | `juce::TabBarButton` | Barra de navegação por abas contextuais |
| **Bottom Deck** | `deck_playlist` | `juce::Component` | Painel de multi-instrumentos e playlists |
| **Bottom Deck** | `deck_effects` | `juce::Component` | Rack de efeitos e inserções DSP |
| **Bottom Deck** | `deck_logic` | `juce::Component` | Editor de nós lógicos e transições |
| **Bottom Deck** | `deck_spatializer` | `juce::Component` | Radar polar 3D e atenuação de distância |
| **Bottom Deck** | `deck_mixer` | `juce::Component` | Visão completa da console do mixer |
| **Bottom Deck** | `deck_diagnostics` | `juce::Component` | Console de mensagens e logs do sistema |
| **Inspector** | `inspector_volume` | `juce::Slider` | Volume da entidade selecionada |
| **Inspector** | `inspector_pitch` | `juce::Slider` | Afinação (pitch) da entidade selecionada |
| **Inspector** | `inspector_pan` | `juce::Slider` | Balanço panorâmico (pan) |
| **Inspector** | `inspector_max_inst` | `juce::Slider` | Limite de instâncias simultâneas |

Para os cabeçalhos de faixa da Timeline:
- `studio_ui_inspect` com `{"component_id": "timeline_view"}` retorna cada `track_*_<i>` (`0..N-1`) com `toggle_state` fiel em `track_mute_<i>`, `track_solo_<i>` e `track_auto_<i>`.
- `studio_trigger_control` com `{"component_id": "track_mute_0"}` ou `{"component_id": "track_solo_0"}` alterna o estado exatamente uma vez e aplica a mixagem imediatamente ao bus da faixa em audição.
- `studio_ui_set_control` permite estado idempotente em botões (`{"component_id": "track_mute_0", "toggle": true}`) e ajuste ao vivo em sliders (`{"component_id": "track_volume_0", "value": -12.0}` ou `{"component_id": "track_pan_0", "value": -1.0}`).

---

## 7. Verificação Automatizada

Para validar localmente se o servidor MCP está íntegro e respondendo a todas as chamadas de inicialização, ferramentas, recursos e socket TCP na porta 7451, execute o smoke test dedicado:

```powershell
.\build\Studio_Release\OpenAudioStudio_artefacts\Release\OpenAudioStudio.exe --smoke-mcp
```

Saída esperada:
```
[OAE Studio] Starting --smoke-mcp validation (EPICO 6: MCP Server)...
[OAE Studio] MCP initialize verified.
[OAE Studio] MCP tools/list verified (found 6 tools).
[OAE Studio] MCP resources/list verified (found 3 resources).
[OAE Studio] MCP resource oae://project/state verified.
[OAE Studio] MCP resource oae://audio/meters verified.
[OAE Studio] MCP tool studio_select_item verified.
[OAE Studio] MCP tool studio_set_mixer_fader verified with RFC 6902 patch diff.
[OAE Studio] MCP tool studio_move_clip verified with RFC 6902 patch diff.
[OAE Studio] MCP tool studio_transport (play) verified.
[OAE Studio] MCP tool studio_transport (stop) verified.
[OAE Studio] MCP tool studio_build_bank verified.
[OAE Studio] MCP resource oae://diagnostics/log verified.
[OAE Studio] MCP TCP Socket Server (127.0.0.1:7451) verified.
[OAE Studio] --smoke-mcp PASSED with exit code 0.
```
