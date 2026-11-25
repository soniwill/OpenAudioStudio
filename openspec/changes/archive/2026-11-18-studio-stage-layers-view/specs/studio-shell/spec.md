# Spec Delta

## ADDED Requirements

### Requirement: Alternância de modo de visualização entre Timeline e Layers no palco
O `TimelinePanel` SHALL disponibilizar na sua barra superior um seletor segmentado entre os modos `Timeline` e `Layers`, mantendo o modo `Timeline` como visualização padrão e preservando integralmente sua geometria, controles, régua, mini-mapa e comportamento existente. A alternância para o modo `Layers` MUST manter visível a `TimelineParameterStrip` no topo do palco para simulação imediata de parâmetros.

#### Scenario: Modo Timeline preservado como padrão ao abrir evento
- **WHEN** o Studio abre um projeto ou seleciona um evento pela primeira vez na sessão
- **THEN** o palco exibe o modo `Timeline` por padrão com suas pistas temporais, régua e mini-mapa inalterados
- **AND** o seletor segmentado indica `Timeline` como ativo e `Layers` disponível para alternância

#### Scenario: Alternância para o modo Layers exibe a pilha de camadas mantendo a fita de parâmetros
- **WHEN** o usuário aciona a opção `Layers` no seletor segmentado do palco
- **THEN** a área central oculta a grade temporal linear e passa a exibir a pilha vertical de camadas (`LayerStackView`) das faixas e ajustes do evento ativo
- **AND** a `TimelineParameterStrip` permanece visível acima da pilha para ajuste ao vivo dos parâmetros do evento

### Requirement: Exibição e controle de stems em cards bipartidos na LayerStackView
Cada faixa de áudio do evento ativo exibida no modo `Layers` SHALL ser apresentada como um card horizontal bipartido contendo, à esquerda, os controles de identidade e mixagem (faixa de cor da pista, botão de visibilidade/Olho, botão Solo, nome do clipe principal, badge de sincronismo/quantização, miniatura da waveform e controle de Opacidade em porcentagem mapeado ao volume da faixa) e, à direita, o resumo e editor da máscara de ativação.

#### Scenario: Renderização bipartida de identidade, waveform e opacidade da camada
- **WHEN** um evento com faixas contendo instrumentos de áudio é visualizado no modo `Layers`
- **THEN** cada faixa é renderizada como um card contendo botão de Olho (visibilidade), botão `S` (Solo), nome, miniatura poligonal da waveform do clipe principal e slider de Opacidade (`0%` a `100%`)
- **AND** ajustar a Opacidade do card atualiza o volume (`volume_db`) da respectiva faixa com registro no histórico de Undo/Redo

#### Scenario: Controle de visibilidade (Olho) e Solo sincronizados com a faixa
- **WHEN** o usuário desativa o botão de Olho ou ativa o botão `S` em um card de camada
- **THEN** o estado de `mute` ou `solo` da faixa correspondente é atualizado imediatamente no modelo do projeto e na audição em tempo real
- **AND** o estado visual do card reflete imediatamente a alteração de forma consistente com o cabeçalho da pista na visão `Timeline`

#### Scenario: Seleção de card na pilha sincroniza o InspectorPanel
- **WHEN** o usuário clica sobre um card de camada na `LayerStackView`
- **THEN** a faixa e o respectivo clipe principal são selecionados no `AuthoringModel`
- **AND** o `InspectorPanel` atualiza sua exibição com os cards detalhados do item selecionado

### Requirement: Máscara visual de ativação por parâmetro e cursor ao vivo alinhado
A coluna direita de cada card na `LayerStackView` SHALL permitir associar um parâmetro do evento, definir a faixa ativa `[min, max]` e o fade de transição na escala normalizada `0.0` a `1.0`, sincronizando a configuração com uma curva de automação de volume da faixa. Todas as barras de máscara SHALL compartilhar o mesmo alinhamento horizontal e exibir uma linha vertical indicando o valor atual do parâmetro ao vivo.

#### Scenario: Configuração de faixa ativa e fade gera curva de automação de volume na faixa
- **WHEN** o usuário seleciona um parâmetro na máscara de uma camada e ajusta a faixa ativa `[min, max]` ou o fade de transição
- **THEN** o Studio cria ou atualiza a curva de automação de `volume` vinculada ao parâmetro na respectiva faixa em uma única transação de Undo/Redo
- **AND** alternar de volta para o modo `Timeline` exibe a curva resultante pronta para edição manual sem perda de dados

#### Scenario: Cursor vertical ao vivo reflete o valor atual do parâmetro em todas as camadas
- **WHEN** o valor de um parâmetro usado pelas máscaras é alterado via `TimelineParameterStrip`, `GlobalParametersDeck` ou Live-Linking
- **THEN** o cursor vertical atravessa a barra de máscara de todas as camadas vinculadas na posição normalizada correspondente ao valor atual
- **AND** cada card atualiza o badge de estado em tempo real (`AGUARDANDO` ou `TOCANDO` com percentual efetivo)

#### Scenario: Preservação de curvas de automação complexas preexistentes
- **WHEN** uma faixa possui uma curva de automação de volume editada manualmente que não corresponde ao formato padrão de máscara em faixa única
- **THEN** a coluna direita do card indica `Curva Personalizada` com botão de atalho para abrir a edição completa na `Timeline` sem sobrescrever os pontos existentes automaticamente

### Requirement: Criação de camadas de stem e importação por drag-and-drop na LayerStackView
A `LayerStackView` SHALL oferecer ações para adicionar novas camadas diretamente no fluxo de stems, incluindo botão `+ Stem Layer` e importação por arrastar e soltar arquivos de áudio suportados (`.wav`, `.ogg`, `.mp3`, `.flac`), configurando o novo clipe de stem para reprodução contínua em loop.

#### Scenario: Botão Add Stem Layer cria nova faixa em loop
- **WHEN** o usuário aciona o botão `+ Stem Layer` na barra do modo `Layers`
- **THEN** uma nova faixa com um instrumento preparado para stem em loop é adicionada ao evento ativo em um passo único de Undo/Redo
- **AND** o novo card passa a aparecer selecionado na pilha de camadas

#### Scenario: Arrastar arquivo de áudio para a LayerStackView cria camada de stem configurada para loop
- **WHEN** o usuário arrasta um arquivo de áudio válido (`.wav`, `.ogg`, `.mp3`, `.flac`) do sistema operacional e o solta sobre a `LayerStackView`
- **THEN** o arquivo é copiado para `assets/` caso necessário, registrado nos samples do projeto e associado a uma nova faixa com `Single Instrument` configurado com `loop = true`
- **AND** a operação inteira é registrada como um passo único de Undo/Redo
