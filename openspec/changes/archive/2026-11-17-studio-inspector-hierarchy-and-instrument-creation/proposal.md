# Proposal

## Why

Atualmente no OpenAudio Studio, a experiência de inspeção e criação de elementos na Timeline apresenta atritos ergonômicos e quebras da hierarquia natural de contenção (*containment hierarchy*):
1. Ao clicar em um Evento (no navegador de assets ou abas), o `InspectorPanel` força uma seleção implícita para a Faixa 0 (`active_track = 0`), renderizando o `TrackCard` no topo e empurrando o `EventHeaderCard` para o rodapé. Isso gera confusão sobre qual entidade está selecionada e sobre a quem pertencem os instrumentos.
2. A criação de instrumentos está restrita ao botão no `TrackCard` (que sempre insere na posição `0.0 ms`) ou arraste externo de arquivos. O clique com botão direito em áreas vazias da faixa na Timeline (`TimelineTrackLane`) é descartado, impedindo a criação ágil de instrumentos na posição temporal desejada.
3. Clicar em uma área vazia da faixa não seleciona a faixa correspondente no modelo de autoria.

Esta mudança estabelece o princípio canônico de que **a ação de adicionar um elemento reside no seu pai imediato** (`Evento -> Tracks`, `Track -> Instrumentos`, `Instrumento -> Sons/Playlist`), garante que o card correspondente ao item selecionado apareça em primeiro lugar no Inspector e viabiliza a criação contextual de instrumentos diretamente na Timeline no tempo exato do clique.

## What Changes

* **Hierarquia e Ordenação de Cards no `InspectorPanel`**:
  * Eliminação do fallback forçado de `active_track = 0` quando `track_index == -1`.
  * Ao selecionar um **Evento**: o `EventHeaderCard` é renderizado como o card primário no topo da pilha, exibindo propriedades de voz, prioridade, cooldown, fadeout e as ações no pai imediato (`+ Add Track` e `+ Add Local Param`). Nenhum card de faixa é renderizado quando não houver faixa selecionada.
  * Ao selecionar uma **Faixa** (`track_index >= 0`, `instrument_index == -1`): o `TrackCard` é renderizado no topo com os controles de fader, pan, solo, mute e a ação no pai imediato (`+ Add Inst`), com o `EventHeaderCard` posicionado abaixo de forma recolhida (*collapsed*).
  * Ao selecionar um **Instrumento/Clip** (`track_index >= 0`, `instrument_index >= 0`): o `ClipCard` é renderizado no topo com suas propriedades de áudio, loop, pitch e modulações, com os cards ancestrais (`TrackCard` e `EventHeaderCard`) recolhidos abaixo.
* **Criação e Seleção Contextual na `TimelineTrackLane`**:
  * Clique com botão esquerdo em área vazia da faixa da Timeline seleciona a faixa no `AuthoringModel` (`ItemSelection{ItemType::Event, path, track_index, -1}`), atualizando o Inspector para a faixa clicada.
  * Clique com botão direito em área vazia da faixa da Timeline abre o menu de contexto da faixa com a opção `Add Instrument >` (Single, Multi, Scatterer, Programmer, Event, Command, Silence), criando o instrumento no tempo exato (`t_ms`) correspondente à coordenada horizontal clicada (`px_to_time_`).
* **Suporte a Posição Temporal no `AuthoringModel`**:
  * Extensão do método `AuthoringModel::add_instrument` para aceitar um parâmetro opcional `double offset_ms = 0.0`, posicionando o clipe criado na coordenada temporal solicitada.

## Capabilities

### New Capabilities
*(Nenhuma nova capacidade; as mudanças evoluem a autoria e interface do Studio).*

### Modified Capabilities
- `studio-shell`: Adição de requisitos para ordenação hierárquica e foco no alvo selecionado no `InspectorPanel` (card do evento no topo ao selecionar evento, card de faixa ao selecionar faixa, card de clipe ao selecionar clipe) e criação contextual de instrumentos via menu de clique direito na `TimelineTrackLane` com posicionamento temporal preciso.

## Impact

* **Camada de Autoria / C++ Studio**:
  * `studio/Source/UI/InspectorPanel.cpp` e `studio/Source/UI/InspectorPanel.h`: reformulação da lógica de montagem de cards em `create_event_editor` baseada no nível estrito de `ItemSelection`.
  * `studio/Source/UI/TimelineTrackLane.cpp` e `studio/Source/UI/TimelineTrackLane.h`: tratamento de clique esquerdo (seleção de faixa) e menu contextual de clique direito em área vazia (cálculo de `t_ms` e chamada de criação).
  * `studio/Source/Model/AuthoringModel.h` e `studio/Source/Model/AuthoringModel.cpp`: assinatura e implementação de `add_instrument` com `offset_ms`.
* **Sem quebras de compatibilidade**: O formato binário do banco, o esquema JSON do projeto e a ABI C nativa permanecem inalterados.
* **Documentação**: Atualização do manual offline do Studio nos capítulos de Timeline e Inspector e evidências de verificação via MCP.
