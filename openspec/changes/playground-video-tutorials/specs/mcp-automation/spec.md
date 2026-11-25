# Spec Delta — mcp-automation

## ADDED Requirements

### Requirement: Destaque visual programático de controle via MCP (`studio_ui_highlight`)
O servidor MCP do OpenAudio Studio SHALL expor a ferramenta `studio_ui_highlight` na Message Thread aceitando `component_id`, `label`, `dim_opacity` (`0.0..0.85`, padrão `0.45`), `color` (`cyan`, `amber` ou `green`, padrão `cyan`) e `clear` (booleano, padrão `false`), ativando, atualizando ou limpando o destaque visual sobre o controle alvo.

#### Scenario: Ativação do destaque por `component_id` válido com rótulo opcional
- **WHEN** o cliente MCP invoca `studio_ui_highlight` com um `component_id` visível e layoutado e `clear` falso ou omitido
- **THEN** o servidor ativa o destaque sobre o retângulo do componente alvo nas coordenadas locais da janela e retorna `isError: false`
- **AND** a resposta JSON contém `active: true`, `component_id`, `bounds` (`x`, `y`, `w`, `h`), `label`, `dim_opacity` e `color`

#### Scenario: Limpeza explícita do destaque via `clear: true`
- **WHEN** o cliente MCP invoca `studio_ui_highlight` com `clear: true`
- **THEN** o servidor desativa e oculta o overlay de destaque, redesenha o `root_component` e retorna `isError: false` com `active: false`

#### Scenario: Rejeição determinística de alvo ausente, inexistente ou sem área
- **WHEN** o cliente MCP invoca `studio_ui_highlight` com `clear` falso para um `component_id` omitido, não encontrado ou com largura ou altura menor ou igual a zero
- **THEN** o servidor retorna `isError: true` com mensagem diagnóstica clara e preserva estado válido e consistente no overlay

#### Scenario: Inclusão do destaque ativo em captura completa da janela
- **WHEN** o cliente MCP invoca `studio_ui_screenshot` omitindo `component_id` enquanto o destaque está ativo
- **THEN** o PNG gerado para o `root_component` inclui a máscara de escurecimento, o contorno de destaque e o rótulo visíveis na interface
