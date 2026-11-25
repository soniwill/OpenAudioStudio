# Spec Delta

## ADDED Requirements

### Requirement: Adição e exibição de target_event no PlaylistDeck
O `PlaylistDeck` do OpenAudio Studio SHALL permitir adicionar entradas de evento (`target_event`) além de samples em playlists de `Multi` e `Scatterer`, listando apenas eventos válidos (sem ciclos) e identificando entradas de evento com o prefixo `[EVT]` na lista.

#### Scenario: Adição de evento na playlist de Multi Instrument
- **WHEN** o usuário aciona o botão de adição no `PlaylistDeck` para um `Multi Instrument` e escolhe um evento alvo válido
- **THEN** o `AuthoringModel` insere `{ "target_event": "<event_path>", "weight": 1.0 }` na playlist com suporte a Undo/Redo
- **AND** a lista do `PlaylistDeck` exibe a entrada identificada com `[EVT]` e o nome do evento

#### Scenario: Omissão de eventos que criam ciclo no menu de adição
- **WHEN** o evento atual ou um ancestral direto/indireto existe no projeto
- **THEN** o menu de adição de eventos no `PlaylistDeck` não exibe eventos que criariam ciclo na playlist

### Requirement: Autoria e visualização de Programmer Instrument no Studio
O OpenAudio Studio SHALL permitir criar um `Programmer Instrument` pelo menu `+ Add Instrument`, editar `programmer_key` no `InspectorPanel` (com seleção a partir da `audio_table` do projeto ou texto livre) e visualizar o clipe identificado com `[PRG]` na `TimelineTrackLane`.

#### Scenario: Criação e edição de Programmer Instrument com Undo/Redo
- **WHEN** o usuário adiciona um `Programmer Instrument` em uma pista e define `programmer_key` no `InspectorPanel`
- **THEN** o `AuthoringModel` persiste `kind: "programmer"` e `programmer_key` com suporte a Undo/Redo
- **AND** a `TimelineTrackLane` renderiza o clipe identificado com `[PRG]` e a chave configurada

### Requirement: Edição da audio_table no OpenAudio Studio
O OpenAudio Studio SHALL fornecer controles de autoria para gerenciar entradas da `audio_table` do projeto (adicionar, remover e associar chave a `sample_id`) com suporte a Undo/Redo para que `Programmer Instruments` possam ser pré-visualizados no Studio.

#### Scenario: Associação de chave na audio_table a um sample
- **WHEN** o usuário cria ou edita uma entrada da `audio_table` ligando uma chave única a um `sample_id` do projeto
- **THEN** o `AuthoringModel` persiste a entrada em `audio_table` com suporte a Undo/Redo
- **AND** a chave fica disponível para seleção no Inspector de `Programmer Instrument` e na compilação de prévia/publicação
