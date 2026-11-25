## ADDED Requirements

### Requirement: Sub-lanes automáticas para clipes sobrepostos na timeline
A timeline do OpenAudio Studio SHALL distribuir clipes da mesma pista em sub-lanes verticais sempre que seus intervalos temporais `[offset_ms, offset_ms + length_ms)` se sobrepuserem, garantindo que nenhum clipe oculte outro visualmente. Quando não houver sobreposição com outros clipes da pista, o clipe MUST ocupar a altura útil completa da pista.

#### Scenario: Clipes sobrepostos na mesma pista
- **WHEN** dois instrumentos na mesma pista possuem intervalos temporais sobrepostos (por exemplo, um `Event Instrument` em `[0, 500]` ms e um `Single Instrument` em `[400, 2000]` ms)
- **THEN** `TimelineTrackLane` atribui sub-lanes verticais distintas aos clipes sobrepostos
- **AND** nenhum clipe é desenhado sobre o waveform do outro

#### Scenario: Clipes sem sobreposição na mesma pista
- **WHEN** os instrumentos da mesma pista ocupam intervalos temporais disjuntos
- **THEN** cada clipe ocupa a altura útil completa da pista sem divisão em sub-lanes

### Requirement: Representação visual de Event Instrument com badge e waveform resumo
Um `ClipComponent` do tipo `event` SHALL exibir um badge curto `[EVT] <nome-do-evento>` utilizando o segmento final do caminho do evento alvo e SHALL renderizar um waveform resumo obtido a partir dos instrumentos de áudio (`Single`) do evento filho referenciado, com fallback visual limpo quando o evento filho não possuir áudio direto ou estiver vazio.

#### Scenario: Event Instrument apontando para evento com Single Instrument
- **WHEN** um `Event Instrument` referencia um evento alvo que contém pelo menos um `Single Instrument` com sample válido
- **THEN** o clipe exibe o rótulo `[EVT] <nome-do-evento>`
- **AND** o interior do clipe desenha o waveform resumo correspondente ao sample do evento filho

#### Scenario: Event Instrument apontando para evento sem sample direto
- **WHEN** um `Event Instrument` referencia um evento vazio ou composto apenas por `Multi`/`Command` ou sem alvo definido
- **THEN** o clipe exibe o rótulo `[EVT] <nome-do-evento>` (ou `[EVT] (No target)`) e um placeholder visual limpo sem erro de renderização

### Requirement: Navegação por duplo-clique em Event Instrument
Dar duplo-clique em um `ClipComponent` do tipo `event` com `target_event` válido SHALL selecionar o evento alvo no `AuthoringModel`, abrindo-o para inspeção e edição no Studio.

#### Scenario: Duplo-clique abre o evento filho
- **WHEN** o usuário executa duplo-clique em um clipe `Event Instrument` cujo `target_event` existe no projeto
- **THEN** a seleção do `AuthoringModel` é atualizada para `ItemType::Event` apontando para o `target_event`
