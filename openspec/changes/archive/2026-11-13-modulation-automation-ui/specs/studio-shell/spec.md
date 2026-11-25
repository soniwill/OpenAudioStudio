# Spec Delta

## ADDED Requirements

### Requirement: Seção Modulation no InspectorPanel com gráfico AHDSR interativo
O `InspectorPanel` do OpenAudio Studio SHALL exibir uma seção **Modulation** para Eventos e Instrumentos selecionados, permitindo adicionar, editar e remover moduladores `RANDOM` e `AHDSR`, com gráfico interativo de envelope AHDSR e Undo/Redo por gesto.

#### Scenario: Criação e ajuste de AHDSR no InspectorPanel
- **WHEN** o usuário seleciona um evento ou instrumento no `InspectorPanel`, adiciona um modulador `AHDSR` e ajusta `Attack`, `Hold`, `Decay`, `Sustain`, `Release` ou `Key Off` pelos sliders ou arraste no gráfico
- **THEN** o `AuthoringModel` registra a alteração como um gesto único de Undo/Redo e dispara a reconstrução da prévia

### Requirement: Editor de Curvas de Automação por Parâmetro no InspectorPanel
O `InspectorPanel` SHALL fornecer um editor visual de curvas de automação por parâmetro para eventos/faixas e efeitos de bus, permitindo escolher parâmetro local/global, alvo (`Volume`, `Pitch`, `Pan` ou parâmetro de efeito), manipular `1..32` pontos no gráfico e selecionar a forma de segmento (`LINEAR`, `HOLD`, `EASE_IN`, `EASE_OUT`, `S_CURVE`).

#### Scenario: Criação e edição de curva dirigida por parâmetro
- **WHEN** o usuário cria uma curva de automação vinculada a um parâmetro do projeto no `InspectorPanel` e edita seus pontos e forma de segmento
- **THEN** o gráfico e o modelo mantêm os pontos ordenados em `x` com suporte a Undo/Redo por gesto e compilação limpa pelo `oae_bankc`
