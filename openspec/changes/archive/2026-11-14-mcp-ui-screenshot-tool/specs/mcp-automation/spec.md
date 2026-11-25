# mcp-automation

## Purpose

Definir os contratos de automação, inspeção semântica e captura visual do OpenAudio Studio via Model Context Protocol (MCP) sobre stdio e TCP, permitindo que agentes de IA e suítes de smoke test inspecionem e operem a DAW de forma determinística em modo GUI ou headless.

## ADDED Requirements

### Requirement: Captura de screenshot da janela ou componente via MCP (`studio_ui_screenshot`)
O servidor MCP do OpenAudio Studio SHALL expor a ferramenta `studio_ui_screenshot`, que renderiza em memória um snapshot bitmap (`juce::Image`) da árvore de componentes JUCE na Message Thread via `juce::Component::createComponentSnapshot` e o codifica em formato PNG (`juce::PNGImageFormat`), funcionando tanto com janela de desktop ativa quanto em host headless (`HeadlessUiHost`).

#### Scenario: Captura da janela completa por padrão
- **WHEN** o cliente MCP invoca `studio_ui_screenshot` sem informar `component_id` (ou com `component_id` vazio)
- **THEN** o servidor captura o `root_component` completo na Message Thread com fator de escala padrão `1.0`
- **AND** retorna `isError: false` com metadados JSON (`width`, `height`, `scale`, `byte_size`) e os dados da imagem PNG gerada

#### Scenario: Captura escopada a um componente específico por `component_id`
- **WHEN** o cliente MCP invoca `studio_ui_screenshot` informando um `component_id` válido (como `timeline_view`, `deck_spatializer` ou `deck_effects`)
- **THEN** o servidor localiza recursivamente o componente alvo, renderiza apenas a área local daquele componente em PNG e retorna suas dimensões exatas (`width` e `height`) nos metadados

#### Scenario: Rejeição determinística de `component_id` inexistente
- **WHEN** o cliente MCP invoca `studio_ui_screenshot` com um `component_id` que não existe na hierarquia do `root_component`
- **THEN** o servidor retorna `isError: true` com mensagem diagnóstica indicando o `component_id` não encontrado, sem gerar arquivo inválido

#### Scenario: Rejeição de componente alvo sem área renderizável
- **WHEN** o componente resolvido possui largura ou altura menor ou igual a zero (`width <= 0 || height <= 0`)
- **THEN** o servidor retorna `isError: true` com mensagem diagnóstica informando que o componente não possui área visível/layoutado

### Requirement: Persistência em disco e controle de payload base64 no `studio_ui_screenshot`
A ferramenta `studio_ui_screenshot` SHALL suportar os parâmetros opcionais `output_path` (caminho de arquivo `.png` para gravação em disco), `scale` (fator de escala limitado ao intervalo `[0.25, 2.0]`, padrão `1.0`) e `include_base64` (booleano, padrão `true` quando `output_path` é omitido e `false` quando `output_path` é fornecido, podendo ser sobrescrito explicitamente).

#### Scenario: Salvamento direto de PNG em disco via `output_path`
- **WHEN** o cliente MCP invoca `studio_ui_screenshot` fornecendo `output_path` apontando para um caminho gravável
- **THEN** o servidor cria os diretórios pais necessários, grava o arquivo PNG com cabeçalho válido (`89 50 4E 47`) e inclui o caminho absoluto resolvido em `output_path` no JSON de resposta

#### Scenario: Retorno de bloco de conteúdo `image` base64 no protocolo MCP
- **WHEN** `include_base64` é efetivamente `true` na chamada a `studio_ui_screenshot`
- **THEN** o array `content` da resposta MCP inclui um item `{ "type": "image", "mimeType": "image/png", "data": "<base64>" }` além do item `{ "type": "text", "text": "<json_metadata>" }`
