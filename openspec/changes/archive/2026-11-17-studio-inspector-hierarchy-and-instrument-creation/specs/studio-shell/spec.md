# Spec Delta

## MODIFIED Requirements

### Requirement: Agrupamento modular em cards colapsáveis por escopo (Clip, Track e Event) no InspectorPanel
O `InspectorPanel` SHALL organizar os parâmetros de autoria respeitando o nível ativo de seleção (`ItemSelection`), posicionando no topo o card do elemento diretamente selecionado e suprimindo faixas não selecionadas quando apenas o evento estiver sob foco, mantendo subgrupos lógicos internos e identificação visual por cor de pista e badge de tipo.

#### Scenario: Seleção de evento exibe exclusivamente o card Event no topo
- **WHEN** o usuário seleciona um evento no navegador de assets ou nas abas do Studio sem pista ou clipe selecionado
- **THEN** o `InspectorPanel` exibe o card **Event** (`EventHeaderCard`) no topo da pilha
- **AND** nenhum card de pista (`TrackCard`) ou de clipe (`ClipCard`) é exibido no painel

#### Scenario: Seleção de pista exibe o card Track no topo
- **WHEN** o usuário seleciona uma pista específica na Timeline ou no cabeçalho de pistas
- **THEN** o `InspectorPanel` exibe o card **Track** (`TrackCard`) no topo da pilha
- **AND** o card **Event** (`EventHeaderCard`) é renderizado abaixo em estado recolhido (*collapsed*)
- **AND** nenhum card de clipe (`ClipCard`) é exibido enquanto não houver clipe selecionado

#### Scenario: Seleção de clipe exibe cards agrupados em ordem contextual
- **WHEN** o usuário seleciona um clipe em uma pista no palco ou no navegador
- **THEN** o `InspectorPanel` apresenta o card colapsável **Clip** no topo (com subgrupos lógicos de fonte/disparo, fades/tempo e modulação do clipe), seguido pelo card **Track** da pista selecionada e pelo card **Event** na base
- **AND** outras pistas e clipes não selecionados do mesmo evento não são listados simultaneamente no painel

#### Scenario: Alternância e colapso independente de cards no Inspector
- **WHEN** o usuário recolhe ou expande o cabeçalho de qualquer card (`Clip`, `Track` ou `Event`) no `InspectorPanel`
- **THEN** o conteúdo daquele card é recolhido ou expandido preservando o estado dos demais cards e mantendo clara a separação visual entre propriedades do clipe, da pista e do evento

## ADDED Requirements

### Requirement: Criação contextual de instrumentos e seleção de pista na Timeline
A `TimelineTrackLane` SHALL permitir selecionar a pista e criar novos instrumentos diretamente sobre a raia temporal, calculando a posição de início (`offset_ms`) a partir da coordenada horizontal do cursor do mouse.

#### Scenario: Clique com botão esquerdo em área vazia seleciona a pista
- **WHEN** o usuário clica com o botão esquerdo do mouse em uma área vazia da raia de uma pista na Timeline
- **THEN** o Studio seleciona aquela pista no `AuthoringModel` com índice de clipe nulo
- **AND** o `InspectorPanel` atualiza seu conteúdo para exibir o `TrackCard` da pista clicada

#### Scenario: Menu contextual de clique direito para adicionar instrumento na posição do cursor
- **WHEN** o usuário clica com o botão direito do mouse em uma área vazia da raia de uma pista na Timeline
- **THEN** o Studio exibe um menu contextual com o submenu `Add Instrument` listando os tipos suportados (`Single`, `Multi`, `Scatterer`, `Programmer`, `Event`, `Command`, `Silence`)
- **AND** ao acionar um tipo de instrumento, o novo clipe é criado na pista correspondente com `offset_ms` correspondente à posição temporal do clique

### Requirement: Contenção hierárquica das ações de criação de elementos de áudio
As ações de criação e adição de novos elementos no Studio SHALL residir estritamente no card de inspeção e no contexto do seu pai imediato na árvore de contenção do projeto.

#### Scenario: Card de Evento contém ações de criação de faixas e parâmetros locais
- **WHEN** o card **Event** (`EventHeaderCard`) é exibido no `InspectorPanel`
- **THEN** o card fornece os botões de ação `+ Add Track` (para criar novas faixas filhas no evento) e `+ Add Local Param` (para criar parâmetros locais no evento)
- **AND** não exibe botões diretos de criação de instrumentos de áudio

#### Scenario: Card de Faixa contém ação de criação de instrumentos
- **WHEN** o card **Track** (`TrackCard`) é exibido no `InspectorPanel`
- **THEN** o card fornece a ação `+ Add Inst` com seletor de tipos de instrumento para inserção na respectiva faixa
- **AND** não exibe botões de criação de faixas ou parâmetros de evento

#### Scenario: Card de Instrumento contém ações para itens de playlist
- **WHEN** o card **Clip** (`ClipCard`) de um instrumento de playlist (`Multi` ou `Scatterer`) é exibido no `InspectorPanel`
- **THEN** os controles de adição de arquivos de áudio e eventos (`+ Add Sound` / playlist) residem no próprio card ou no deck de playlist associado ao clipe
