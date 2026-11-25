## ADDED Requirements

### Requirement: Autoria e visualização de Scatterer Instrument no Studio
O OpenAudio Studio SHALL permitir adicionar um `Scatterer Instrument` pelo menu `+ Add Instrument`, editar suas propriedades de spawner (`Spawn Interval Min/Max`, `Polyphony`, `Total Sounds`, `Distance Min/Max`) no `InspectorPanel`, gerenciar sua playlist no `PlaylistDeck` e visualizá-lo na timeline com o identificador `[SCT]`.

#### Scenario: Criação e parametrização de Scatterer Instrument com Undo/Redo
- **WHEN** o usuário adiciona um `Scatterer Instrument` em uma pista e altera `spawn_interval_min_ms`, `spawn_interval_max_ms` ou `polyphony` no Inspector
- **THEN** o `AuthoringModel` persiste as alterações com suporte completo a Undo/Redo
- **AND** a `TimelineTrackLane` renderiza o clipe identificado com `[SCT]` e o `PlaylistDeck` permite adicionar e remover entradas da playlist
