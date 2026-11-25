# Spec Delta

## ADDED Requirements

### Requirement: Edição de User Properties, Tags e Notes de Eventos no Inspector
O `AuthoringModel` e o `InspectorPanel` do OpenAudio Studio MUST permitir adicionar, editar e remover pares chave/valor em `user_props` de eventos, incluindo campos dedicados de anotações (`notes`) e rótulos (`tags`), mantendo compatibilidade com propriedades internas do editor.

#### Scenario: Edição e persistência de User Properties e Notes em evento
- **WHEN** o usuário define `notes`, `tags` ou uma propriedade personalizada chave/valor em um evento pelo Inspector ou `AuthoringModel`
- **THEN** o objeto `user_props` do evento é atualizado com transação de Undo/Redo e preservado na compilação para `.bank`
