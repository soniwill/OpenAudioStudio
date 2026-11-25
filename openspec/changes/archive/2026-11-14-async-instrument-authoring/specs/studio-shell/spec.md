# Spec Delta

## ADDED Requirements

### Requirement: Controle Async para instrumentos no InspectorPanel
Quando um instrumento compatível (`single`, `multi`, `scatterer`, `programmer` ou `event`) estiver selecionado no OpenAudio Studio, o `InspectorPanel` SHALL exibir um controle booleano **Async** refletindo o valor atual de `async` no `AuthoringModel` e permitindo alterná-lo com suporte completo a Undo/Redo.

#### Scenario: Alternar Async no InspectorPanel com Undo/Redo
- **WHEN** o usuário seleciona um instrumento `single` no Studio, ativa o toggle **Async** no `InspectorPanel` e em seguida executa Undo e Redo
- **THEN** a propriedade `async` do instrumento alterna para `true`, retorna para `false` no Undo e volta para `true` no Redo

#### Scenario: Persistência e preview de evento com instrumento Async
- **WHEN** o evento `event:/sfx/charge_sustain_async` do projeto de exemplo é carregado e reproduzido no Studio até o `SustainPoint`
- **THEN** o clipe `async` continua sustentando áudio enquanto o transport permanece em `SUSTAINING` e inicia o release ao acionar **Key Off**
