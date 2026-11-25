# studio-shell Specification

## Purpose

Define os requisitos de interface, timeline, inspetor e fluxos de autoria do aplicativo desktop OpenAudio Studio.

## Requirements

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
Um `ClipComponent` do tipo `event` SHALL exibir um badge curto `[EVT] <nome-do-evento>` utilizando o segmento final do caminho do evento alvo na barra de título e SHALL renderizar um waveform resumo resolvido recursivamente a partir dos instrumentos de áudio (`Single`, `Multi`, `Scatterer`, `Programmer` ou sub-eventos) do evento filho referenciado, com fallback visual limpo apenas quando o evento filho não possuir áudio associável ou estiver vazio.

#### Scenario: Event Instrument apontando para evento com Single Instrument
- **WHEN** um `Event Instrument` referencia um evento alvo que contém pelo menos um `Single Instrument` com sample válido
- **THEN** o clipe exibe o rótulo `[EVT] <nome-do-evento>`
- **AND** o interior do clipe desenha o waveform resumo poligonal correspondente ao sample do evento filho

#### Scenario: Event Instrument apontando para evento com Multi, Scatterer, Programmer ou sub-evento
- **WHEN** um `Event Instrument` referencia um evento alvo que contém instrumentos `Multi`, `Scatterer`, `Programmer` (resolvido na `audio_table`) ou sub-eventos com áudio válido
- **THEN** o clipe exibe o rótulo `[EVT] <nome-do-evento>`
- **AND** o interior do clipe desenha o waveform resumo poligonal correspondente ao áudio resolvido do evento filho

#### Scenario: Event Instrument apontando para evento sem sample direto
- **WHEN** um `Event Instrument` referencia um evento vazio, composto apenas por `Command`/`Silence` ou sem alvo/áudio associável definido
- **THEN** o clipe exibe o rótulo `[EVT] <nome-do-evento>` (ou `[EVT] (No target)`) e um placeholder visual limpo sem erro de renderização

### Requirement: Navegação por duplo-clique em Event Instrument
Dar duplo-clique em um `ClipComponent` do tipo `event` com `target_event` válido SHALL selecionar o evento alvo no `AuthoringModel`, abrindo-o para inspeção e edição no Studio.

#### Scenario: Duplo-clique abre o evento filho
- **WHEN** o usuário executa duplo-clique em um clipe `Event Instrument` cujo `target_event` existe no projeto
- **THEN** a seleção do `AuthoringModel` é atualizada para `ItemType::Event` apontando para o `target_event`

### Requirement: Seletor de command_type no Inspector para Command Instrument
Quando um `Command Instrument` estiver selecionado no OpenAudio Studio, o `InspectorPanel` SHALL exibir um seletor **`Command Type`** com as opções `start_event`, `stop_event`, `snapshot_on` e `snapshot_off`, além do seletor **`Command Target`** populado com os alvos válidos para o `command_type` ativo.

#### Scenario: Alternar Command Type entre evento e snapshot
- **WHEN** o usuário altera o **`Command Type`** de `start_event` para `snapshot_on` (ou `snapshot_off`) no Inspector
- **THEN** o modelo atualiza `command_type` do instrumento selecionado com suporte a Undo/Redo
- **AND** o combo **`Command Target`** passa a listar apenas snapshots válidos do projeto via `AuthoringModel::get_instrument_targets(event_path, command_type)`
- **AND** caso o alvo anterior não seja válido para o novo tipo, `command_target` é ajustado para um alvo válido (ou preservado se continuar válido, como na troca entre `start_event` e `stop_event`)

#### Scenario: Alternar para stop_event sem bloqueio de ciclo
- **WHEN** o usuário altera o **`Command Type`** para `stop_event`
- **THEN** o combo **`Command Target`** lista os eventos do projeto sem aplicar a restrição de ciclo de `start_event`

### Requirement: Identificação do command_type na timeline
Um `ClipComponent` do tipo `command` na timeline SHALL indicar seu `command_type` juntamente com o `command_target` no texto exibido no clipe.

#### Scenario: Clipe command exibe tipo e alvo
- **WHEN** um `Command Instrument` configurado como `snapshot_on` com alvo `snapshot:/pause_menu` é renderizado em `TimelineTrackLane`
- **THEN** o rótulo do clipe identifica o tipo `snapshot_on` e o alvo correspondente

### Requirement: Autoria e visualização de Scatterer Instrument no Studio
O OpenAudio Studio SHALL permitir adicionar um `Scatterer Instrument` pelo menu `+ Add Instrument`, editar suas propriedades de spawner (`Spawn Interval Min/Max`, `Polyphony`, `Total Sounds`, `Distance Min/Max`) no `InspectorPanel`, gerenciar sua playlist no `PlaylistDeck` e visualizá-lo na timeline com o identificador `[SCT]`.

#### Scenario: Criação e parametrização de Scatterer Instrument com Undo/Redo
- **WHEN** o usuário adiciona um `Scatterer Instrument` em uma pista e altera `spawn_interval_min_ms`, `spawn_interval_max_ms` ou `polyphony` no Inspector
- **THEN** o `AuthoringModel` persiste as alterações com suporte completo a Undo/Redo
- **AND** a `TimelineTrackLane` renderiza o clipe identificado com `[SCT]` e o `PlaylistDeck` permite adicionar e remover entradas da playlist

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

### Requirement: Manipulação visual direta de itens lógicos na TimelineRuler
A `TimelineRuler` do OpenAudio Studio SHALL fornecer feedback visual ao vivo durante o arraste de marcadores e regiões lógicas, alças laterais de redimensionamento (`length_ms`) para `loop_region` e `transition_region`, rótulos informativos (destino, quantização, repetições/KeyOff e indicador de condição), além de criação e remoção direta via menu de contexto e tecla `Delete`.

#### Scenario: Arraste ao vivo e resize lateral de região lógica na régua
- **WHEN** o usuário arrasta uma `loop_region` ou `transition_region` pela área central ou puxa sua alça lateral na `TimelineRuler`
- **THEN** a régua exibe o deslocamento ou novo comprimento ao vivo durante o arraste e aplica a alteração ao `AuthoringModel` no `mouseUp` como um único passo de Undo/Redo

#### Scenario: Criação e remoção via menu de contexto na TimelineRuler
- **WHEN** o usuário abre o menu de contexto na `TimelineRuler`
- **THEN** o menu oferece a criação de `Marker`, `Tempo Marker`, `Loop Region`, `Transition Marker`, `Transition Region` e `Sustain Point`, além da opção `Delete` quando acionado sobre um item lógico existente

### Requirement: Controles completos no LogicConditionsDeck com Undo por gesto
O `LogicConditionsDeck` SHALL permitir criar `transition_region`, configurar `until_key_off` em `loop_region`, ajustar `crossfade_ms` em transições, editar o nome de `marker` e os campos `bpm`/`ts_upper`/`ts_lower` de `tempo`, agrupando arrastes de sliders em gestos únicos de Undo/Redo.

#### Scenario: Edição de Until Key Off, Crossfade e Tempo no LogicConditionsDeck
- **WHEN** o usuário seleciona uma `loop_region` e ativa `Until Key Off`, seleciona uma `transition_region` e ajusta `Crossfade (ms)`, ou seleciona um marcador `tempo` e altera `BPM` e fórmula de compasso
- **THEN** os controles habilitam-se contextualmente conforme o tipo selecionado, atualizam o `AuthoringModel` e registram arrastes contínuos de sliders como um único gesto de Undo/Redo

### Requirement: Controle Async para instrumentos no InspectorPanel
Quando um instrumento compatível (`single`, `multi`, `scatterer`, `programmer` ou `event`) estiver selecionado no OpenAudio Studio, o `InspectorPanel` SHALL exibir um controle booleano **Async** refletindo o valor atual de `async` no `AuthoringModel` e permitindo alterná-lo com suporte completo a Undo/Redo.

#### Scenario: Alternar Async no InspectorPanel com Undo/Redo
- **WHEN** o usuário seleciona um instrumento `single` no Studio, ativa o toggle **Async** no `InspectorPanel` e em seguida executa Undo e Redo
- **THEN** a propriedade `async` do instrumento alterna para `true`, retorna para `false` no Undo e volta para `true` no Redo

#### Scenario: Persistência e preview de evento com instrumento Async
- **WHEN** o evento `event:/sfx/charge_sustain_async` do projeto de exemplo é carregado e reproduzido no Studio até o `SustainPoint`
- **THEN** o clipe `async` continua sustentando áudio enquanto o transport permanece em `SUSTAINING` e inicia o release ao acionar **Key Off**

### Requirement: Edição de User Properties, Tags e Notes de Eventos no Inspector
O `AuthoringModel` e o `InspectorPanel` do OpenAudio Studio MUST permitir adicionar, editar e remover pares chave/valor em `user_props` de eventos, incluindo campos dedicados de anotações (`notes`) e rótulos (`tags`), mantendo compatibilidade com propriedades internas do editor.

#### Scenario: Edição e persistência de User Properties e Notes em evento
- **WHEN** o usuário define `notes`, `tags` ou uma propriedade personalizada chave/valor em um evento pelo Inspector ou `AuthoringModel`
- **THEN** o objeto `user_props` do evento é atualizado com transação de Undo/Redo e preservado na compilação para `.bank`

### Requirement: Elevação 3D, Envelopment e Atalhos no Spatializer3DDeck
O `Spatializer3DDeck` do OpenAudio Studio SHALL suportar ajuste de elevação 3D (`emitter_y_`) com `Ctrl + arraste` vertical, rotação por roda do mouse, duplo-clique no radar para redefinir o emissor na origem e sliders `Sound Size (m)` e `Min Extent (deg)` sincronizados com a API espacial 3D da instância local em audição.

#### Scenario: Controles de elevação e envelopment atualizam o preview 3D local
- **WHEN** o usuário manipula os sliders `spatial_preview_sound_size` e `spatial_preview_min_extent`, ou utiliza `Ctrl + drag`, roda do mouse e duplo-clique no radar do `Spatializer3DDeck`
- **THEN** o deck exibe as coordenadas X/Y/Z atualizadas no rótulo de estado e aplica `oae_event_instance_set_3d_attributes` e `oae_event_instance_set_spatializer_settings` à instância local ativa

### Requirement: Governança visual unificada e tokens de design no Studio
O OpenAudio Studio SHALL aderir às diretrizes de interface especificadas em `docs/UI_GUIDELINES.md`, aplicando cores semânticas, escala tipográfica e espaçamentos múltiplos de 4px de forma consistente em todos os painéis e diálogos através do tema centralizado.

#### Scenario: Consistência visual e temática nos painéis
- **WHEN** qualquer painel do Studio (Navegador, Timeline, Inspetor, Mixer ou Decks) for renderizado na tela
- **THEN** suas cores de superfície, bordas, textos e acentos funcionais MUST derivar exclusivamente dos tokens de tema definidos no Design System.

### Requirement: Controles rotatórios vetoriais com arco proporcional e indicador central
Os controles do tipo rotary slider (knobs) no OpenAudio Studio SHALL ser renderizados inteiramente via primitivas vetoriais com arco proporcional ao valor e indicador central nítido, sem depender de imagens bitmap estáticas.

#### Scenario: Renderização vetorial responsiva
- **WHEN** um knob de parâmetro, ganho ou efeito for exibido ou redimensionado
- **THEN** o Studio MUST desenhar o fundo do trilho, o arco de valor preenchido e o ponteiro central usando geometria vetorial nítida em qualquer fator de escala da janela.

### Requirement: Ajuste fino com Shift e reset por duplo-clique em controles numéricos
Os sliders e knobs do OpenAudio Studio SHALL reduzir a sensibilidade de arraste do mouse para ajuste fino quando a tecla `Shift` for mantida pressionada e SHALL restaurar o valor padrão (`default_value`) do parâmetro ao receber um duplo-clique.

#### Scenario: Ajuste fino com tecla Shift pressionada
- **GIVEN** um controle numérico (knob ou slider) sendo arrastado pelo usuário com o mouse
- **WHEN** a tecla `Shift` estiver pressionada durante o arraste
- **THEN** a sensibilidade de deslocamento MUST ser reduzida em relação ao deslocamento padrão para permitir ajuste fino com precisão de décimos de unidade.

#### Scenario: Restauração de valor padrão por duplo clique
- **WHEN** o usuário der um duplo-clique sobre um knob ou slider que possua valor padrão definido
- **THEN** o controle MUST retornar imediatamente ao seu `default_value` e registrar a alteração no histórico de Undo/Redo.

### Requirement: Medidores de nível dBFS vetoriais com decaimento balístico
Os medidores de nível de áudio e de pico (VU meters) do Studio SHALL ser desenhados vetorialmente e atualizados a uma taxa de 30 a 60 FPS com decaimento balístico suave e indicador de retenção de pico (peak hold).

#### Scenario: Exibição balística durante reprodução
- **GIVEN** um evento de áudio em reprodução ativa emitindo sinal para um bus ou fader de pista
- **WHEN** o sinal de áudio oscilar em nível sonoro
- **THEN** o medidor dBFS MUST atualizar suas barras vetoriais com animação de subida instantânea e queda balística amortecida, mantendo o indicador de pico visível temporariamente.

### Requirement: Layout fluido sem coordenadas fixas nos painéis internos
Os painéis internos do OpenAudio Studio SHALL organizar seus elementos filhos utilizando exclusivamente modelos de layout flexíveis e responsivos (`FlexBox` ou `Grid`), redistribuindo os controles fluidamente ao redimensionar a janela ou os divisores sem truncar componentes.

#### Scenario: Redimensionamento fluido de painéis e barras
- **WHEN** a janela principal ou um divisor de área (`SplitterBar`) for movido pelo usuário
- **THEN** os painéis afetados MUST readequar o espaçamento, quebras de linha e dimensões de seus botões e campos através de FlexBox/Grid sem sobreposição de controles ou limites manuais fixos soltos.

### Requirement: Reorganização de painéis por abas e janelas destacáveis (Pop-out)
O sistema de layout do OpenAudio Studio SHALL permitir reorganizar painéis por arraste do título, agrupá-los em abas ou criar divisões horizontais e verticais redimensionáveis e aninhadas. O Studio SHALL indicar a área de encaixe antes da soltura e permitir destacar painéis em janelas flutuantes independentes com retorno ao dock.

#### Scenario: Destacar painel em janela flutuante
- **WHEN** o usuário acionar o comando ou botão de destaque (*pop-out*) em um painel do Studio
- **THEN** o painel MUST ser removido do grupo ancorado e aberto em uma janela flutuante redimensionável e operável simultaneamente ao restante da aplicação.

#### Scenario: Retorno de janela destacada ao dock principal
- **WHEN** a janela flutuante destacada for fechada ou o comando de retorno for acionado
- **THEN** o painel MUST retornar à posição e ao grupo de ancoragem original, reconstituindo a divisão se necessário quando a estrutura de origem ainda estiver disponível.

#### Scenario: Agrupar ou reordenar abas pelo título
- **WHEN** o usuário arrasta o título de um painel sobre uma barra de abas e solta em uma posição válida
- **THEN** a prévia MUST indicar a posição de inserção e o painel MUST aparecer nessa posição no grupo de destino
- **AND** a instância do painel MUST sair da posição anterior e ocupar uma única posição no workspace.

#### Scenario: Dividir um painel ou a área de docking
- **WHEN** o usuário arrasta uma aba para uma borda de outro painel ou para uma borda externa da área de docking
- **THEN** o Studio MUST mostrar a área de encaixe proposta antes da soltura
- **AND** ao soltar, MUST criar uma divisão redimensionável no eixo correspondente, abrangendo o painel de destino ou a área de docking, respectivamente.

#### Scenario: Cancelar um arraste
- **WHEN** o usuário pressiona Esc durante o arraste ou solta sem um alvo de encaixe válido
- **THEN** a prévia MUST desaparecer e a posição, seleção e visibilidade dos painéis MUST permanecer como antes do gesto.

### Requirement: Barra de breadcrumb para navegação entre eventos pai e filho na Timeline
O `TimelinePanel` do OpenAudio Studio SHALL exibir o caminho hierárquico de navegação quando um evento aninhado (`Event Instrument`) for aberto a partir de um evento pai, permitindo retornar a qualquer ancestral clicando no respectivo segmento do breadcrumb ou no botão de voltar.

#### Scenario: Abrir evento filho por duplo-clique e retornar ao pai pelo breadcrumb
- **WHEN** o usuário abre um evento filho dando duplo-clique em um clipe `[EVT]` na timeline de um evento pai
- **THEN** a barra superior da timeline exibe a trilha `evento-pai > evento-filho` com segmentos clicáveis
- **AND** ao clicar no segmento do evento pai ou no botão de retorno, a seleção do `AuthoringModel` e a timeline voltam para o evento pai

### Requirement: Renderização vetorial de formas de curva e gradiente nas pistas e editores de automação
As pistas de automação da timeline (`AutomationLaneComponent`) e os editores de curva (`AutomationCurveEditor`, `AhdsrCurveEditor` e `EqPlot`) SHALL desenhar os segmentos conforme a forma configurada (`linear`, `hold`, `ease_in`, `ease_out`, `s_curve`) com preenchimento vertical em gradiente translúcido sob a curva e nós vetoriais destacados no hover/seleção.

#### Scenario: Renderização de segmento não linear e preenchimento sob a curva na timeline
- **WHEN** uma pista de automação contém pontos com forma `ease_in`, `ease_out`, `s_curve` ou `hold`
- **THEN** a `AutomationLaneComponent` desenha a trajetória correspondente entre os pontos usando primitivas vetoriais suaves e preenche a área sob a curva com gradiente translúcido derivado de `OaeTheme`

### Requirement: Edição direta por arraste de pontos na pista de automação da Timeline
A faixa `Volume Automation` da Timeline SHALL exibir e editar a curva temporal de volume da pista em milissegundos e decibéis. O editor SHALL permitir arrastar pontos existentes e consolidar o movimento completo como uma única operação de Undo/Redo. A faixa SHALL permitir inserir pontos por duplo clique e SHALL preservar as demais curvas da pista.

#### Scenario: Arrastar ponto de automação na timeline com Undo/Redo por gesto
- **WHEN** o usuário arrasta um ponto da curva temporal de volume e solta o botão do mouse
- **THEN** o tempo e o valor do ponto são atualizados ao vivo durante o arraste e registrados como um único passo de Undo/Redo ao final do gesto

#### Scenario: Editar a curva temporal de volume em uma pista com outras automações
- **WHEN** uma pista contém uma curva temporal de volume e outras curvas, em qualquer ordem, e o usuário abre a faixa pelo botão `A`
- **THEN** a faixa exibe apenas os pontos da curva temporal de volume
- **AND** selecionar, arrastar ou inserir pontos na faixa preserva as demais curvas, inclusive as curvas por parâmetro exibidas no Inspector

#### Scenario: Criar a curva temporal de volume em uma pista que só tem outras automações
- **WHEN** o usuário abre a faixa de uma pista que tem outras automações e ainda não tem curva temporal de volume
- **THEN** a faixa exibe a grade vazia e a abertura preserva os dados da pista
- **WHEN** o usuário dá duplo clique na grade
- **THEN** a faixa cria uma curva temporal de volume com o ponto inserido nas coordenadas correspondentes ao gesto
- **AND** as curvas existentes mantêm seus pontos, origem, alvo e ordem

### Requirement: Exibição da faixa de automação pelo cabeçalho da pista
O botão `A` de cada pista da timeline SHALL alternar a visibilidade da sua faixa de automação abaixo dos clipes. A timeline SHALL manter o destaque do botão e a altura do cabeçalho e da pista sincronizados com essa visibilidade, sem alterar os dados do projeto.

#### Scenario: Expandir e recolher uma pista com curva existente
- **WHEN** o usuário clica no botão `A` de uma pista recolhida com uma curva de automação de volume cadastrada
- **THEN** o botão fica destacado e a faixa de automação aparece abaixo dos clipes, com a curva e seus pontos editáveis
- **AND** as outras pistas mantêm seu estado de expansão
- **WHEN** o usuário clica novamente no mesmo botão
- **THEN** o destaque é removido, a faixa de automação é ocultada e a pista volta à altura recolhida
- **AND** os pontos da curva permanecem disponíveis ao expandir novamente

#### Scenario: Expandir uma pista sem curva
- **WHEN** o usuário clica no botão `A` de uma pista recolhida sem curva de automação
- **THEN** a faixa de automação aparece com rótulo e grade vazia
- **AND** o clique não cria pontos ou curvas no projeto

### Requirement: Tokens semânticos e micro-layout FlexBox/Grid na Timeline, Headers e Decks Contextuais
Os componentes `TimelinePanel`, `TimelineRuler`, `TimelineTrackLane`, `TrackHeaderPanel`, `PlaylistDeck`, `LogicConditionsDeck`, `Spatializer3DDeck` e `EffectChainDeck` SHALL utilizar os tokens de cores, tipografia e espaçamento de `OaeTheme` e organizar suas barras e formulários internos via `juce::FlexBox` ou `juce::Grid`.

#### Scenario: Redimensionamento compacto de cabeçalhos de pista e decks contextuais
- **WHEN** a largura ou altura útil da timeline e do deck contextual inferior for reduzida pelo usuário
- **THEN** a toolbar da timeline, os itens de `TrackHeaderPanel` e os controles internos de `PlaylistDeck`, `LogicConditionsDeck`, `Spatializer3DDeck` e `EffectChainDeck` se redistribuem via `FlexBox`/`Grid` sem truncamento nem sobreposição

### Requirement: Rastreamento 1:1 do cursor no thumb de sliders lineares
Os sliders lineares do OpenAudio Studio SHALL mover o indicador visual (bolinha/thumb) em proporção `1:1` com o deslocamento do ponteiro sobre a extensão física útil do trilho durante o arraste normal, mantendo a geometria do thumb alinhada ao trilho e aplicando o fator fino (`0.1x`) apenas enquanto `Shift` estiver pressionado.

#### Scenario: Arraste horizontal sem atraso em slider linear
- **WHEN** o usuário clica sobre o thumb ou trilho de um slider linear horizontal e arrasta o ponteiro por `N` pixels dentro da extensão útil do trilho sem pressionar `Shift`
- **THEN** o centro do thumb acompanha exatamente a posição horizontal percorrida pelo ponteiro sem salto inicial e sem depender de sensibilidade fixa de 250 pixels

#### Scenario: Preservação do ajuste fino com Shift pressionado
- **WHEN** o usuário mantém a tecla `Shift` pressionada durante o arraste de um slider linear
- **THEN** o deslocamento do valor passa a aplicar o fator fino de `0.1x` em relação ao movimento do ponteiro a partir da posição atual do thumb

### Requirement: Agrupamento modular em cards colapsáveis por escopo (Clip, Track e Event) no InspectorPanel
O `InspectorPanel` SHALL organizar as propriedades em cards colapsáveis (`Clip`, `Track`, `Event` e, quando relevante, `MÁSCARA & TRANSIÇÃO`) respeitando `ItemSelection`, adotando estilo `ValueBox` e tokens Figma em ambos os modos (`Timeline` e `Layers`), exibindo metadados reais de sample quando disponíveis e preservando a ordem contextual dos cards.

#### Scenario: Seleção de evento exibe exclusivamente o card Event no topo
- **WHEN** o usuário seleciona um evento no navegador de assets ou nas abas do Studio sem pista ou clipe selecionado
- **THEN** o `InspectorPanel` exibe o card **Event** (`EventHeaderCard`) no topo da pilha com linhas estilo `ValueBox` (`bgSurface`, borda `borderSubtle`)
- **AND** nenhum card de pista (`TrackCard`) ou de clipe (`ClipCard`) é exibido no painel

#### Scenario: Seleção de pista exibe o card Track no topo
- **WHEN** o usuário seleciona uma pista específica na Timeline ou na `LayerStackView`
- **THEN** o `InspectorPanel` exibe o card **Track** (`TrackCard`) no topo da pilha
- **AND** o card **Event** (`EventHeaderCard`) é renderizado abaixo em estado recolhido (*collapsed*)
- **AND** nenhum card de clipe (`ClipCard`) é exibido enquanto não houver clipe selecionado

#### Scenario: Seleção de clipe exibe cards agrupados em ordem contextual
- **WHEN** o usuário seleciona um clipe em uma pista no palco ou no navegador
- **THEN** o `InspectorPanel` apresenta o card colapsável **Clip** no topo (com metadados reais de sample quando decodificados e subgrupos lógicos em linhas `ValueBox`), seguido pelo card **Track** da pista selecionada e pelo card **Event** na base
- **AND** outras pistas e clipes não selecionados do mesmo evento não são listados simultaneamente no painel

#### Scenario: Exibição contextual da seção MÁSCARA & TRANSIÇÃO
- **WHEN** o usuário seleciona uma pista ou clipe de evento com o palco no modo `Layers`, ou quando a faixa selecionada possui máscara de camada ativa (`LayerMaskSpec`)
- **THEN** o `InspectorPanel` exibe a seção recolhível `MÁSCARA & TRANSIÇÃO` com seletor de parâmetro, limites normalizados `[min, max]`, fade normalizado existente (sem curva ou duração fictícia e sem campo `Mesclagem`) e atalho para editar a curva na `Timeline`
- **AND** quando a faixa está sempre ativa no modo `Timeline`, a seção `MÁSCARA & TRANSIÇÃO` permanece oculta

#### Scenario: Alternância e colapso independente de cards no Inspector
- **WHEN** o usuário recolhe ou expande o cabeçalho de qualquer card (`Clip`, `Track`, `Event` ou `MÁSCARA & TRANSIÇÃO`) no `InspectorPanel`
- **THEN** o conteúdo daquele card é recolhido ou expandido preservando o estado dos demais cards e mantendo clara a separação visual entre os escopos

### Requirement: Cores por pista aplicadas aos clipes e destaque visual de pista e clipe selecionados no palco
Cada pista do evento SHALL possuir uma cor identificadora atribuída automaticamente e editável pelo usuário, aplicada ao background de todos os seus clipes na timeline, enquanto a pista selecionada recebe destaque contínuo de ponta a ponta e o clipe selecionado recebe realce em alto contraste, diferenciando o tipo de instrumento exclusivamente pelo badge na barra de título.

#### Scenario: Diferenciação visual da pista selecionada quando há múltiplas pistas
- **WHEN** um evento possui duas ou mais pistas e o usuário seleciona uma pista no cabeçalho ou no palco
- **THEN** a pista ativa exibe destaque contínuo conectando o cabeçalho (`TrackHeaderPanel`) e a raia do palco (`TimelineTrackLane`) com realce translúcido da cor da pista e bordas superior e inferior em evidência

#### Scenario: Cor da pista no background do clipe e destaque do clipe selecionado
- **WHEN** clipes de diferentes tipos (`Single`, `Multi`, `Scatterer`, `Programmer`, `Event`, `Command`) pertencem a uma pista com cor definida
- **THEN** o background de todos os clipes dessa pista utiliza a cor da pista e diferencia o tipo apenas pelo badge/ícone na barra de título do clipe
- **AND** o clipe selecionado no palco destaca-se com barra de título iluminada em alto contraste, moldura dupla brilhante e alças laterais visíveis

### Requirement: Renderização poligonal de waveforms para todos os tipos de instrumentos e eventos no palco
O palco da timeline SHALL renderizar waveforms vetoriais poligonais em duas camadas (silhueta de pico com gradiente vertical e núcleo RMS interno) para clipes `Single`, `Multi`, `Scatterer`, `Programmer` e `Event` sempre que houver áudio associável no clipe, na playlist ou na `audio_table`.

#### Scenario: Exibição de waveform em Multi, Scatterer e Programmer Instrument
- **WHEN** um clipe do tipo `Multi`, `Scatterer` ou `Programmer` possui ao menos um sample válido em sua playlist ou mapeado por `programmer_key` na `audio_table`
- **THEN** o corpo do clipe no palco desenha a waveform poligonal correspondente sem permanecer vazio

#### Scenario: Renderização poligonal suave com camada de pico e RMS
- **WHEN** qualquer clipe com waveform válida é pintado na `TimelineTrackLane`
- **THEN** a forma de onda é desenhada com contorno poligonal contínuo anti-aliased, preenchimento em gradiente vertical da camada de pico e núcleo RMS interno contrastado sobre o background colorido pela pista

### Requirement: Pré-visualização de geometria fantasma (Ghost Geometry) durante o arraste de clipe no palco
Durante todo o arraste de um clipe no palco, tanto na mesma pista quanto entre pistas diferentes, a timeline SHALL exibir continuamente a geometria fantasma do clipe acompanhando a posição do mouse, preservando uma referência visual na posição de origem até a conclusão do gesto.

#### Scenario: Arraste horizontal na mesma pista com geometria fantasma
- **WHEN** o usuário arrasta um clipe horizontalmente dentro da mesma pista
- **THEN** a timeline exibe continuamente o ghost do clipe (contorno, silhueta da waveform, linhas-guia verticais até a régua e indicador temporal) acompanhando o movimento do ponteiro enquanto mantém o contorno tracejado na posição original

#### Scenario: Arraste vertical entre pistas com geometria fantasma
- **WHEN** o usuário arrasta um clipe sobre outra pista na timeline
- **THEN** a pista de destino renderiza a geometria fantasma completa do clipe alinhada ao tempo sob o ponteiro antes de efetivar a mudança no `mouseUp`

### Requirement: Reconfiguração de painéis, ocultação total e gerenciamento de templates e presets de layout
O sistema de docking do Studio SHALL permitir reconfigurar o encaixe de cada painel, ocultar ou restaurar painéis via cinco pills (`Trn`, `Brw`, `Stg`, `Ins`, `Dck`) na `WorkspaceBar` e menu `☰`/`…` (abrigando `Log`, `Prj`, `Hide All / Restore`, `Dock`, `Delete` e `Reset`), alternar os templates nativos e salvar/excluir layouts personalizados.

#### Scenario: Ocultação individual e ocultação total de painéis com restauração imediata
- **WHEN** o usuário alterna um dos cinco pills (`Trn`, `Brw`, `Stg`, `Ins`, `Dck`) na `WorkspaceBar` ou aciona `Log`, `Prj` ou `Hide All / Restore` no menu `☰`/`…` da barra de workspace
- **THEN** os painéis ocultos deixam de ocupar área de tela, os painéis visíveis expandem-se para preencher o espaço livre e a `WorkspaceBar` permanece acessível com pills em `tintCyan` e borda `accentCyan` para painéis ativos
- **AND** a ação `Restore` após `Hide All` MUST recuperar as divisões e os grupos de abas anteriores

#### Scenario: Aplicação de templates nativos, criação, exclusão e reset de layouts
- **WHEN** o usuário seleciona um template nativo (`Default (Authoring)`, `Stage Focus` ou `Mixing & Mastering`) no seletor `Preset:`, aciona `+ Save Preset` ou escolhe `Delete Custom Preset` / `Reset Layout` no menu `☰`/`…`
- **THEN** o workspace MUST aplicar o estado correspondente, registrar a configuração e os presets personalizados em `oae_studio_layout.json` e proteger os templates nativos contra exclusão

#### Scenario: Salvar e restaurar o layout de referência
- **WHEN** o usuário monta o layout de referência, altera um divisor, salva como preset, troca de layout, reinicia o Studio e volta a selecionar o preset salvo
- **THEN** o Studio MUST restaurar as divisões, proporções, posições, visibilidade, ordem e aba ativa de cada grupo salvos, respeitando os tamanhos mínimos da janela atual
- **AND** Inspector, Bottom Deck e Diagnostics & Log MUST voltar a aparecer nas posições relativas do layout de referência

#### Scenario: Abrir um layout de slots salvo antes da mudança
- **WHEN** o Studio abre um arquivo de layout válido de formato anterior à mudança
- **THEN** o workspace MUST restaurar o estado e os presets legíveis com geometria equivalente, incluindo dimensões, visibilidade e agrupamento conforme os dados disponíveis

### Requirement: Layout de referência com Inspector lateral e logs independentes

O Studio SHALL disponibilizar `Diagnostics & Log` como painel encaixável com identidade própria e permitir montar o layout de referência com o Inspector em toda a altura da área de docking, à direita. À esquerda, os logs SHALL ocupar uma divisão imediatamente abaixo do Bottom Deck, com a mesma largura dele.

#### Scenario: Consultar logs enquanto usa o Bottom Deck

- **WHEN** o usuário encaixa o Inspector na borda direita externa e `Diagnostics & Log` abaixo do Bottom Deck no espaço restante à esquerda
- **THEN** o Inspector MUST continuar visível da parte superior à inferior da área de docking, abaixo da barra do workspace
- **AND** os limites esquerdo e direito de `Diagnostics & Log` MUST coincidir com os do Bottom Deck
- **AND** alternar entre Mixer, Effects ou outras abas internas MUST manter os logs visíveis.

#### Scenario: Ajustar o espaço dos logs

- **WHEN** o usuário arrasta o divisor acima dos logs no layout de referência
- **THEN** o Bottom Deck e os logs MUST redistribuir a altura respeitando os tamanhos mínimos aplicáveis
- **AND** a coluna do Inspector MUST conservar seus limites.

### Requirement: Cabeçalhos de docking em grafite
O Studio SHALL usar cabeçalhos únicos `DockHeader` (`24 px`, `dockHeaderBg`) com título em caixa alta e metadados à direita nos painéis isolados (sem cabeçalho duplo), e `DockTab`s (`22 px`, `rounded=3 px`) nos grupos de abas e `BottomDeckHeader` (`34 px`) destacando a aba ativa com `dockTabActiveBg`, borda `accentCyan` e foco de teclado visível.

#### Scenario: Distinguir título, seleção e foco
- **WHEN** o usuário compara um dock com painel único (`BROWSER`, `STAGE`, `PROPERTY INSPECTOR`) com uma barra de abas (`BrowserTabs`, `BottomDeckHeader` ou grupo multi-aba) e navega pelo teclado
- **THEN** o painel único MUST exibir um único cabeçalho grafite (`24 px`) com título neutro à esquerda e metadado/controle auxiliar à direita sem barra duplicada
- **AND** em grupos de abas, a aba ativa MUST destacar-se das inativas com borda `accentCyan` e contraste de fundo/texto mantendo o foco de teclado perceptível independentemente da seleção

### Requirement: Encerramento normal do Studio com banco de prévia carregado

O Studio SHALL encerrar normalmente quando o usuário fechar a janela com um projeto aberto e um banco de prévia carregado. O processo SHALL concluir a liberação dos recursos da sessão e terminar com código de saída zero.

#### Scenario: Fechar o Studio com o projeto de exemplo aberto

- **WHEN** o usuário abre o projeto de exemplo, aguarda a confirmação de que a prévia está pronta e fecha a janela principal
- **THEN** o processo MUST terminar com código de saída zero
- **AND** ao abrir o mesmo projeto em uma nova sessão, a prévia MUST voltar a ficar disponível.

### Requirement: Navegação e integração do manual offline e páginas conceituais de fundamentos
O OpenAudio Studio SHALL disponibilizar o manual oficial em formato estático acessível offline via menu Help e atalho F1, estruturado com páginas dedicadas de fundamentos conceituais no início da documentação que detalham a ontologia de eventos, parâmetros, buses, VCAs e snapshots com diagramas de fluxo e evidências visuais do aplicativo.

#### Scenario: Acesso às páginas de fundamentos pelo índice do manual
- **WHEN** o usuário abre o manual offline do Studio via menu Help, atalho F1 ou navegação local em `index.html`
- **THEN** a barra lateral e a página inicial exibem links destacados para os capítulos de fundamentos teóricos de eventos/parâmetros e mixagem/buses/vcas/snapshots
- **AND** todas as páginas carregam estilos locais, diagramas e imagens sem dependência de conexão com a internet

#### Scenario: Referências cruzadas nos capítulos operacionais
- **WHEN** o usuário consulta um capítulo procedural de interface (como timeline, inspetor ou barramentos)
- **THEN** o capítulo exibe caixas de contextualização vinculando os controles visíveis aos conceitos arquiteturais detalhados nas páginas de fundamentos

#### Scenario: Ilustração de conceitos com capturas reais do Studio
- **WHEN** o usuário lê as explicações sobre tiras do mixer, hierarquia de barramentos, snapshots ou propriedades do inspetor
- **THEN** a documentação apresenta capturas visuais da interface geradas a partir do ambiente do Studio anotando os controles correspondentes

### Requirement: Cobertura profunda e exemplos de uso do Bottom Deck no manual offline
O manual offline do OpenAudio Studio SHALL detalhar teoria acústica, motivação de design, parâmetros e exemplos práticos de `Mixer`, `Effect Chain`, `Logic Conditions`, `Playlist`, `3D Spatializer`, `Parameters (N)` e `Diagnostics`, com capturas atualizadas. `Diagnostics` SHALL ser descrito como aba quando embutido e painel encaixável quando destacado.

#### Scenario: Consulta teórica aos processadores DSP e medição
- **WHEN** o usuário acessa a documentação de `Mixer` ou `Effect Chain` no capítulo 7
- **THEN** o manual apresenta a motivação acústica e as faixas de operação dos sete efeitos DSP nativos, a balística de medição Peak/RMS e K-System e as práticas de headroom para evitar clipping na soma do Master

#### Scenario: Consulta à estocástica e lógica adaptativa
- **WHEN** o usuário consulta `Playlist` ou `Logic Conditions`
- **THEN** o manual detalha seleção estocástica, pesos, marcadores de transição, quantização e regiões de loop
- **AND** os exemplos mostram uma playlist com sample e evento e uma transição com marcador de destino, condição e resultado esperado

#### Scenario: Consulta à espacialização 3D e telemetria
- **WHEN** o usuário consulta `3D Spatializer` ou `Diagnostics`
- **THEN** o manual explica as curvas de distância e o envelopment da prévia local, a configuração de cones do emissor Unity e como interpretar as mensagens de prévia, compilação e conexão
- **AND** a absorção atmosférica aparece como conceito acústico e a contagem de vozes é localizada na ferramenta que a exibe, como o overlay da Unity

#### Scenario: Consultar logs enquanto alterna o Bottom Deck
- **WHEN** o usuário segue o procedimento para montar o layout com Inspector lateral e `Diagnostics` abaixo do Bottom Deck
- **THEN** encontra o painel pelo menu `☰`/`…` da `WorkspaceBar`, consegue encaixá-lo em uma divisão independente e mantém as mensagens visíveis ao alternar as abas internas do Deck
- **AND** o manual mostra como reabrir painéis ocultos e restaurar ou salvar o layout

### Requirement: Documentação atualizada da Workspace Bar, timeline e layout do Inspector
O manual offline do OpenAudio Studio SHALL documentar a `WorkspaceBar` (5 pills, `Preset:`, `+ Save Preset` e menu `☰`/`…`), a `TopToolbarRow`, o navegador por abas com rodapé de parâmetros, o cabeçalho da Stage + `LIVE PARAMS` + `LayerStackView` plana e o Inspector com `ValueBox` e `MÁSCARA & TRANSIÇÃO`, mantendo as capturas alinhadas ao tema atual.

#### Scenario: Documentação da barra de workspace e templates
- **WHEN** o usuário consulta os capítulos 1 e 3 do manual sobre visão geral da interface e transporte
- **THEN** o manual descreve os 5 pills (`Trn`, `Brw`, `Stg`, `Ins`, `Dck`), os templates nativos e `+ Save Preset`, o menu `☰`/`…` (`Log`, `Prj`, `Hide All / Restore`, `Dock`, `Delete`, `Reset Layout`) e os novos controles da `ProjectToolbar` e `TransportBar`
- **AND** as páginas apresentam capturas de tela atualizadas da janela e das barras alinhadas ao Figma

#### Scenario: Documentação de navegação por breadcrumbs e cores de pista
- **WHEN** o usuário consulta os capítulos 4 e 5 sobre navegador, timeline e camadas
- **THEN** o manual documenta as 4 abas (`Events`, `Snapshots`, `Buses` com `VCAs`, `Assets`), o rodapé `PROJECT PARAMETERS (N)`, a sub-barra exclusiva do modo `Timeline`, a faixa `LIVE PARAMS` e a barra horizontal plana com rampas translúcidas na `LayerStackView`

#### Scenario: Documentação dos cards do Inspector e atalhos de slider
- **WHEN** o usuário consulta o capítulo 6 sobre o painel de propriedades
- **THEN** o manual descreve o cabeçalho único `PROPERTY INSPECTOR` com cadeado `Lock`, os cards `ValueBox`, a seção contextual `MÁSCARA & TRANSIÇÃO`, o menu `...` dos cards, o ajuste fino com `Shift` e o reset por duplo-clique

### Requirement: Documentação de instrumentos avançados, parameter sheets e integração Unity no manual offline

O manual offline do OpenAudio Studio SHALL documentar a autoria e operação de Command, Scatterer e Programmer, a opção Async, Parameter Sheets (PSHT) e modificadores de parâmetros (PMOD). SHALL descrever a integração de StudioEventEmitter e StudioListener com cones, atenuação e os cinco Built-in Parameters 3D e apresentar capturas atuais dos aplicativos envolvidos.

#### Scenario: Documentação de Command Instruments e Parameter Sheets na Timeline
- **WHEN** o usuário consulta Timeline, Inspector ou os tutoriais correspondentes
- **THEN** o manual explica os tipos e alvos disponíveis de Command e demonstra o disparo com recursos identificados
- **AND** mostra como vincular um parâmetro existente a um instrumento por Trigger Sheet, configurar Sheet Min e Sheet Max e verificar o disparo ao entrar no intervalo inclusivo e o encerramento ou release ao sair dele
- **AND** diferencia os intervalos de disparo da automação de ganho usada para suavizar transições.

#### Scenario: Documentação de Async e instrumentos com seleção de áudio
- **WHEN** o usuário consulta Async, Scatterer ou Programmer
- **THEN** encontra um exemplo da opção Async de um instrumento compatível com Sustain Point e Key Off, a configuração de intervalo de spawn e polifonia do Scatterer e um exemplo de Programmer com chave associada a um sample
- **AND** a documentação distingue a reprodução via tabela de áudio da resolução personalizada por callback.

#### Scenario: Documentação de componentes Unity e espacialização 3D física
- **WHEN** o usuário segue o exemplo de espacialização na Unity
- **THEN** encontra a configuração de StudioEventEmitter e StudioListener, o uso de um listener ativo por sistema, os ajustes de cone, distância e envelopment e o cálculo automático de Distance, Direction, Elevation, EventConeAngle e Speed
- **AND** o exemplo indica como comparar a prévia local e o resultado no jogo, incluindo uma comparação do ganho dentro e fora do cone à mesma distância.

#### Scenario: Substituição e consistência de imagens da documentação
- **WHEN** o usuário consulta os procedimentos revisados de workspace, Timeline, Inspector, integração Unity ou barramentos
- **THEN** as figuras usadas nesses procedimentos correspondem ao build ou Editor de referência e mostram o estado que suas legendas descrevem.

### Requirement: Posicionamento da audição local pela régua com o transporte parado

O Studio SHALL guardar a posição escolhida na régua de tempo do evento atual enquanto a audição local está parada, refletir essa posição nos indicadores de tempo e usá-la como ponto inicial da próxima audição local do evento. O Studio SHALL limpar a posição ao acionar Stop ou ao trocar o evento de audição.

#### Scenario: Escolha de posição com a audição parada

- **WHEN** o usuário clica ou arrasta em uma área da régua de tempo livre de itens lógicos com a audição local parada
- **THEN** o ponteiro da régua, o minimapa e os contadores mostram a posição resultante do gesto, com as regras de snap da régua
- **AND** os indicadores permanecem nessa posição nas atualizações seguintes, com a audição ainda parada

#### Scenario: Início da audição na posição escolhida

- **WHEN** o usuário pressiona Play local após selecionar uma posição válida diferente de zero na régua com o transporte parado
- **THEN** a audição local começa nessa posição e o ponteiro acompanha o avanço da reprodução

#### Scenario: Limpeza pelo comando Stop

- **WHEN** o usuário aciona Stop depois de escolher uma posição com a audição parada ou durante a audição local iniciada desse ponto
- **THEN** o ponteiro, o minimapa e os contadores retornam a zero e o próximo Play local começa do início, salvo nova escolha na régua

#### Scenario: Troca do evento de audição

- **WHEN** o usuário muda o evento de audição depois de escolher uma posição com o transporte parado
- **THEN** os indicadores do evento recém-selecionado mostram zero e seu primeiro Play local começa do início, salvo nova escolha na régua

### Requirement: Exclusão de pontos na faixa de automação da Timeline

A faixa `Volume Automation` SHALL permitir excluir um ponto individual por `Delete`/`Backspace` com a faixa focada e pelo menu de contexto do ponto. Excluir o último ponto SHALL remover a curva temporal de volume da pista, devolver a faixa à grade vazia e preservar as demais curvas da pista. Cada exclusão SHALL ocupar um único passo de Undo/Redo.

#### Scenario: Excluir um ponto preservando as demais curvas

- **WHEN** a faixa `Volume Automation` está focada, um ponto está selecionado e a pista contém outras automações
- **THEN** `Delete` ou `Backspace` remove apenas o ponto selecionado
- **AND** as demais curvas mantêm origem, alvo, ordem e pontos
- **AND** um único `Ctrl+Z` restaura o ponto removido e um `Ctrl+Y` o exclui novamente

#### Scenario: Excluir o último ponto remove a curva temporal

- **WHEN** a curva temporal de volume da pista tem um único ponto e o usuário o exclui pela faixa
- **THEN** a curva temporal de volume é removida da pista
- **AND** a faixa volta a exibir a grade vazia
- **AND** as outras automações da pista permanecem inalteradas e na mesma ordem
- **AND** um único `Ctrl+Z` restaura a curva com o ponto original

#### Scenario: Excluir um ponto pelo menu de contexto

- **WHEN** o usuário clica com o botão direito sobre um ponto da faixa e escolhe a opção de exclusão
- **THEN** o ponto é removido com o mesmo resultado e o mesmo passo único de Undo/Redo da exclusão por teclado

#### Scenario: Tecla de exclusão sem ponto selecionado

- **WHEN** a faixa `Volume Automation` está focada e não há ponto selecionado nem sob o ponteiro
- **THEN** `Delete` ou `Backspace` não altera pontos, curvas ou clipes

### Requirement: Roteamento da tecla de exclusão entre clipe e faixa de automação

O foco de teclado da Timeline SHALL seguir o último alvo clicado na pista, seja um clipe ou a faixa de automação. Com a faixa focada, `Delete`/`Backspace` SHALL atuar somente sobre a automação; com um clipe como último alvo clicado, a tecla SHALL remover o clipe selecionado como antes.

#### Scenario: Clique no ponto seguido da tecla de exclusão

- **WHEN** o usuário clica em um ponto da faixa e pressiona `Delete`
- **THEN** o ponto é removido
- **AND** nenhum clipe ou item lógico da timeline é removido

#### Scenario: Clique no clipe seguido da tecla de exclusão

- **WHEN** o usuário clica em um clipe depois de interagir com a faixa de automação e pressiona `Delete`
- **THEN** o clipe selecionado é removido como antes
- **AND** os pontos e curvas de automação da pista permanecem inalterados

### Requirement: Acesso por tarefa e busca de procedimentos no manual offline

O manual SHALL oferecer uma entrada por tarefa na página inicial e busca local que leve à seção relevante pelo rótulo do controle, pelo termo do recurso ou por seu equivalente em português. A busca e seus destinos MUST funcionar sem rede na cópia distribuída.

#### Scenario: Entrar pelo objetivo de trabalho

- **WHEN** o leitor abre a página inicial do manual
- **THEN** encontra caminhos diretos para ouvir um primeiro evento, publicar um banco, criar uma transição e configurar um emissor 3D na Unity.

#### Scenario: Encontrar o controle sem conhecer o capítulo

- **WHEN** o leitor pesquisa offline por `Trigger Sheet`, `folha de parâmetros`, `Async`, `Scatterer`, `Command Instrument` ou `Diagnostics & Log`
- **THEN** a busca oferece um link para a seção correspondente com texto que identifica o destino
- **AND** os termos equivalentes com e sem acento retornam o mesmo procedimento.

### Requirement: Procedimentos verificáveis e figuras contextualizadas no manual offline

Os procedimentos revisados SHALL informar o estado inicial, os pré-requisitos, os rótulos dos controles e um resultado visual ou audível. As figuras do aplicativo MUST mostrar o estado usado pelo procedimento e identificar a seleção ou o ajuste relevante.

#### Scenario: Reproduzir um exercício em uma cópia do projeto

- **WHEN** o leitor segue um procedimento com os arquivos indicados no manual
- **THEN** consegue localizar as entradas, efetuar os passos na ordem descrita e comparar o resultado com o estado esperado
- **AND** o manual explica como voltar ao estado inicial quando o exercício altera o projeto ou o layout.

#### Scenario: Comparar o procedimento com uma captura

- **WHEN** o leitor consulta os procedimentos de navegação pai e filho, playlist mista, região de transição ou edição de efeitos
- **THEN** as figuras mostram respectivamente o breadcrumb aninhado, entradas preenchidas, destino e condição configurados e efeito selecionado.

### Requirement: Docks de barra com grip lateral tátil e sem cabeçalho padrão de slot
O Studio SHALL disponibilizar os docks de barra de ferramentas (`Project Toolbar` e `Transport Bar`) com altura compacta unificada (`34px` a `36px`, nominal `36px` sobre superfície `bgSurface`) e um grip lateral tátil na margem esquerda, sem renderizar `SlotHeaderBar`. O grip lateral SHALL permitir arrastar a barra para reordenamento no topo ou destaque flutuante.

#### Scenario: Arraste da barra pelo grip lateral para reposicionamento
- **WHEN** o usuário clica e arrasta o grip lateral tátil de uma barra de ferramentas posicionada no topo
- **THEN** o Studio inicia o gesto de docking com retângulo translúcido de prévia de soltura
- **AND** soltar sobre o topo reposiciona a barra na divisão desejada

#### Scenario: Ausência de cabeçalho tradicional SlotHeaderBar sobre a barra
- **WHEN** uma barra de ferramentas com grip lateral está encaixada no topo da área útil
- **THEN** o Studio não desenha a barra de `SlotHeaderBar` com abas e botões de ação sobre o componente
- **AND** a barra utiliza altura útil contínua de `34px` a `36px` sobre fundo `bgSurface`

### Requirement: Alternância entre modos minimizado e maximizado via grip lateral
O Studio SHALL permitir alternar os docks de barra entre o modo Minimizado (Compacto) e o modo Maximizado (Completo) por duplo-clique no grip lateral ou via menu contextual no botão direito. O modo minimizado do transporte SHALL ocultar `Key Off`, `status`, `Params (N)` e o medidor K-System, exibindo apenas os controles básicos de reprodução e o readout de timecode.

#### Scenario: Duplo-clique no grip lateral alterna para modo compacto
- **WHEN** o usuário executa um duplo-clique sobre a área do grip lateral da `Transport Bar` maximizada
- **THEN** a barra recolhe `Key Off`, status secundário, badge de parâmetros e medidor K-System
- **AND** a barra permanece visível no topo ocupando largura reduzida com botões de transporte e timecode

#### Scenario: Menu de contexto no grip lateral com botão direito
- **WHEN** o usuário clica com o botão direito do mouse sobre o grip lateral de uma barra de ferramentas
- **THEN** o Studio exibe um menu popup com opções para alternar entre modo compacto e expandido e para alternar entre janela flutuante e encaixe no topo

### Requirement: Persistência permanente dos docks de barra sem opção de fechar
Os docks de barra de ferramentas (`Project Toolbar` e `Transport Bar`) SHALL ser permanentes e não exibir botão de fechar ('X'). Quando destacados em janela flutuante, a janela SHALL não exibir botão de fechar do sistema operacional e MUST fornecer uma ação de `Dock Back` para retornar ao topo da interface principal.

#### Scenario: Impossibilidade de fechar o dock de transporte
- **WHEN** o usuário inspeciona a barra de ferramentas no topo ou seus menus de contexto
- **THEN** nenhum botão de fechar ('X') ou comando de fechamento definitivo é disponibilizado para a barra
- **AND** o atalho `[Trn]` na `WorkspaceBar` alterna o foco ou estado da barra em vez de ocultá-la

#### Scenario: Janela flutuante com ação de retorno Dock Back
- **WHEN** a barra de transporte é destacada como janela flutuante independente
- **THEN** a janela flutuante é exibida sem botão de fechar nativo do sistema operacional
- **AND** acionar a ação de `Dock Back` na janela flutuante reintegra a barra ao slot superior da janela principal

### Requirement: Controles com símbolos vetoriais e cores semânticas ativas
A `ProjectToolbar` SHALL exibir a marca `OPENAUDIO`, botões textuais (`New`, `Open`, `Save`, `Build Bank [F7]`) e acesso discreto a `Help (F1)`, enquanto a `TransportBar` SHALL renderizar botões vetoriais `24×24 px` de contorno/tint semântico (`Play`, `Pause`, `Stop`, `Loop` e `Key Off` discreto), readout em caixa `bgDark`, badge `Params (N)` e medidor K-System L/R horizontal.

#### Scenario: Renderização vetorial dos controles de transporte
- **WHEN** a `ProjectToolbar` e a `TransportBar` são renderizadas no topo em modo maximizado
- **THEN** `ProjectToolbar` exibe o rótulo `OPENAUDIO`, os botões `New`, `Open`, `Save`, `Build Bank [F7]` (com borda e texto `accentCyan`) e o atalho discreto `Help (F1)`
- **AND** `TransportBar` exibe botões vetoriais `24×24 px` para `Play`, `Pause`, `Stop`, `Loop` (e `Key Off` discreto) com estilo contorno/tint, `statusLabel` discreto, pill `Params (N)` (`OaeStatusBadge`) e medidor estéreo horizontal `KSystemMeter` alimentado pelo `bus:/`

#### Scenario: Feedback de cor semântica no estado ativo
- **WHEN** o evento ativo entra em reprodução (`is_playing() == true`), pausa ou ativa `Loop`
- **THEN** o botão vetorial correspondente exibe borda, ícone e preenchimento translúcido na cor semântica ativa (`accentGreen` para `Play`, `accentAmber` para `Pause`, `accentCyan`/`tintCyan` para `Loop`)
- **AND** os demais botões mantêm o estilo neutro com borda sutil

#### Scenario: Leitura de timecode e BPM/compasso derivável real no TransportReadout
- **WHEN** o evento ativo possui ao menos um marcador `tempo` em `logic_items`
- **THEN** o `TransportReadout` em caixa `bgDark` exibe `MM:SS.mmm | <bpm> BPM | <ts_upper>/<ts_lower>` conforme o segmento ativo na posição atual
- **AND** quando o evento ativo não possui marcadores `tempo` cadastrados, o readout exibe exclusivamente `MM:SS.mmm` sem inventar BPM ou fórmula de compasso fictícios

### Requirement: Tooltips globais com atalhos de teclado
O Studio SHALL manter ativa uma janela global de tooltips para exibir descrições e atalhos de teclado nos controles da interface ao passar o cursor do mouse sobre botões com símbolos e ações de projeto.

#### Scenario: Exibição de tooltip com atalho de teclado no botão Play
- **WHEN** o cursor do mouse repousa sobre o botão com símbolo vetorial de `Play`
- **THEN** o Studio exibe uma janela de tooltip flutuante com o texto `Play (Space)`

#### Scenario: Exibição de tooltip com atalho no botão Save Project
- **WHEN** o cursor do mouse repousa sobre o botão com símbolo vetorial de `Save Project`
- **THEN** o Studio exibe uma janela de tooltip flutuante com o texto `Save Project (Ctrl+S)`

### Requirement: Suporte a múltiplos formatos de áudio para waveform e metadados na interface
A interface do Studio SHALL suportar a decodificação de arquivos nos formatos WAV, OGG Vorbis, MP3 e FLAC para geração de resumos visuais de waveform (pico e RMS) e leitura de duração e metadados, tanto na visualização de clipes na Timeline quanto na listagem de entradas no deck de playlist.

#### Scenario: Visualização de waveform para arquivos OGG, MP3 e FLAC na Timeline
- **WHEN** um instrumento na Timeline referencia um arquivo de áudio com extensão `.ogg`, `.mp3` ou `.flac`
- **THEN** o Studio decodifica os frames de áudio e renderiza a waveform poligonal de pico e RMS no corpo do clipe sem deixá-lo vazio

#### Scenario: Exibição de duração no deck de playlist para formatos comprimidos
- **WHEN** uma entrada com sample nos formatos OGG, MP3 ou FLAC é exibida na lista de playlist de um instrumento
- **THEN** o deck de playlist apresenta a duração formatada do sample em segundos em vez de marcar como ilegível

### Requirement: Importação de áudio por drag-and-drop direto na Timeline
O Studio SHALL permitir arrastar um ou mais arquivos de áudio válidos (.wav, .ogg, .mp3, .flac) do gerenciador de arquivos do sistema operacional e soltá-los sobre uma pista da Timeline, copiando os arquivos para o diretório de assets, registrando os samples no projeto e criando instrumentos do tipo Single Instrument na posição temporal correspondente.

#### Scenario: Soltar arquivo de áudio sobre pista da Timeline cria Single Instrument
- **WHEN** o usuário arrasta um arquivo de áudio suportado do sistema e o solta sobre uma pista da Timeline
- **THEN** o arquivo é copiado para `assets/` do projeto caso ainda não esteja lá
- **AND** o sample correspondente é cadastrado no `project.json`
- **AND** um novo `Single Instrument` é inserido na pista selecionada na posição de tempo onde o cursor foi solto com a duração original do áudio
- **AND** a operação é registrada como um passo único de Undo/Redo

#### Scenario: Rejeição de arquivos com extensões ou formatos não suportados
- **WHEN** o usuário arrasta arquivos que não correspondam aos formatos de áudio suportados (.wav, .ogg, .mp3, .flac) sobre uma pista da Timeline
- **THEN** o Studio recusa o drop sem criar instrumentos ou alterar os assets do projeto

### Requirement: Importação de áudio por drag-and-drop no navegador de Assets
O Studio SHALL permitir arrastar arquivos de áudio suportados sobre o painel de navegação de assets do projeto, copiando os arquivos para `assets/` e registrando os samples no catálogo do projeto sem criar clipes na Timeline.

#### Scenario: Arrastar múltiplos arquivos de áudio para a árvore de assets
- **WHEN** o usuário arrasta múltiplos arquivos de áudio (.wav, .ogg, .mp3, .flac) e os solta sobre o nó de Audio Assets no navegador do projeto
- **THEN** todos os arquivos são copiados para a pasta `assets/` do projeto
- **AND** os novos samples são adicionados à lista de samples no `project.json`
- **AND** a árvore de assets atualiza sua exibição para listar os novos itens importados

### Requirement: Ação explícita de importação de áudio no menu de contexto de assets
O Studio SHALL disponibilizar a opção "Import Audio..." no menu de contexto da aba `Assets` (sobre itens, cabeçalho ou área livre da árvore de assets), permitindo selecionar um ou mais arquivos de áudio suportados através da caixa de diálogo do sistema operacional.

#### Scenario: Seleção e importação de áudio via diálogo nativo
- **WHEN** o usuário abre a aba `Assets` no `AuthoringTreeBrowser`, aciona a ação "Import Audio..." no menu de contexto e confirma a seleção de arquivos no diálogo
- **THEN** os arquivos selecionados são copiados para `assets/` e registrados como samples no projeto com atualização imediata da aba `Assets`

### Requirement: Bloqueio de seleção (Lock Inspector) no cabeçalho do InspectorPanel
O `InspectorPanel` SHALL fornecer um botão de alternância com ícone de cadeado integrado à direita de seu cabeçalho único de `24 px` (`PROPERTY INSPECTOR`) para travar a inspeção no item atualmente ativo, ignorando mudanças externas de seleção na Timeline, navegador ou mixer enquanto estiver bloqueado.

#### Scenario: Ativação do Lock congela a visualização do item inspecionado
- **WHEN** o usuário seleciona um item (evento, pista, clipe ou barramento) e ativa o botão de cadeado (`inspector_lock_button`) no cabeçalho único de `24 px` do `InspectorPanel`
- **THEN** o botão passa para o estado ativo com destaque visual (`Locked`) sem duplicar cabeçalhos
- **AND** seleções subsequentes na Timeline ou no navegador de projeto não alteram o conteúdo exibido no `InspectorPanel`

#### Scenario: Desativação do Lock sincroniza imediatamente com a seleção atual
- **WHEN** o usuário desativa o botão de cadeado com uma seleção diferente ativa no projeto
- **THEN** o `InspectorPanel` atualiza imediatamente sua visualização para refletir o item selecionado no momento

### Requirement: Ações de contexto e padronização de cabeçalhos nos cards do InspectorPanel
Cada card de inspeção no `InspectorPanel` SHALL possuir um cabeçalho padronizado de `28 px` (`cardHeaderBg`) contendo chevron colapsável, título em caixa alta, badge textual de escopo/estado em `accentCyan` à direita (sem moldura pílula ou faixa lateral espessa) e botão de menu contextual (`...`) com ações de redefinição e cópia.

#### Scenario: Redefinição de valores de um card via menu contextual
- **WHEN** o usuário clica no menu de ações (`...`) no cabeçalho limpo do card e aciona a opção "Reset to Default"
- **THEN** os parâmetros gerenciados por aquele card retornam aos seus valores padrão de fábrica
- **AND** a alteração é consolidada em um único passo no histórico de Undo/Redo do `AuthoringModel`

#### Scenario: Cópia e colagem de propriedades entre cartões compatíveis
- **WHEN** o usuário aciona "Copy Values" no menu de um card e posteriormente "Paste Values" no menu de outro card do mesmo tipo
- **THEN** os parâmetros compatíveis são aplicados ao alvo em uma transação atômica de Undo

### Requirement: Fita Retrátil de Parâmetros Contextuais na Timeline
O `TimelinePanel` SHALL fornecer a faixa horizontal `LIVE PARAMS` (`bgSurfaceRaised`, `42 px` expandida e `24 px` recolhida via botão `[v]`/`[>]`) exibindo exclusivamente os parâmetros locais e globais consumidos pelo evento ativo em pílulas `bgDark`. A faixa SHALL recolher automaticamente para altura zero quando o evento não consome parâmetros.

#### Scenario: Exibição contextual de parâmetros vinculados ao evento
- **WHEN** um evento contendo parâmetros locais ou referências a parâmetros globais em automações, lógica ou sheets é aberto na Timeline
- **THEN** a faixa `LIVE PARAMS` (`42 px`) exibe caixas `bgDark` com nome do parâmetro, slider inline e valor legível (e toggle `[v]`/`[>]` capaz de recolher para `24 px`)
- **AND** duplo-clique sobre qualquer slider restaura o respectivo valor padrão (`default_value`)

#### Scenario: Recolhimento automático para eventos sem parâmetros
- **WHEN** o evento aberto na Timeline não consome nenhum parâmetro local ou global
- **THEN** a fita de parâmetros permanece recolhida com altura zero
- **AND** a régua de tempo e as faixas de áudio ocupam toda a área vertical útil da Timeline

#### Scenario: Parâmetro Labeled exibe texto do estado na fita
- **WHEN** um parâmetro `labeled` com três ou mais opções é exibido na `LIVE PARAMS`
- **THEN** o controle exibe o rótulo textual do estado atual em vez de um índice numérico puro
- **AND** quando um parâmetro `labeled` possui exatamente dois estados nominais (como `False/True` ou `Off/On`), a pílula pode exibir um `StatusBadge` interativo alternando o estado

### Requirement: Deck de Parâmetros Globais no Bottom Deck e Desacoplamento Flutuante
O `ContextualBottomDeck` SHALL exibir cabeçalho único `BottomDeckHeader` (`34 px`) com 7 abas (`Mixer`, `Effect Chain`, `Logic Conditions`, `Playlist`, `3D Spatializer`, `Parameters (N)`, `Diagnostics`) e botão único `Pop-Out ^` (sem botão duplicado em `GlobalParametersDeck`), além de filtro, busca e cards `~131 px` com controles especializados e rodapé real na aba `Parameters (N)`.

#### Scenario: Grid responsivo de cards com controles especializados
- **WHEN** o usuário seleciona a aba `Parameters (N)` no `BottomDeckHeader`
- **THEN** o Studio renderiza os parâmetros globais em cards `~131 px` (`cardBodyBg`) com subtítulo de domínio, `StatusBadge` (`LIVE SEEK`, `TRUE`/`FALSE`, `DEFAULT` ou `MODIFIED`), slider com trilho `6 px`/thumb `14 px` na cor do acento (ou `BoolSegmented` / opções segmentadas para rotulados) e rodapé listando os alvos reais modulados no evento ativo e modificadores (`velocity`/`hold`), sem regras fictícias de Adjustment Layer

#### Scenario: Filtragem rápida e busca de parâmetros
- **WHEN** o usuário digita um termo na caixa de busca (`ParamSearchInput`) ou aciona `All (N)`, `• Used in Event (N)` (destacado em `tintCyan` quando ativo) ou `Modified (N)` na `ParametersFilterBar` (`34 px`)
- **THEN** os contadores refletem os totais atuais do projeto/evento e o grid filtra instantaneamente os cards exibidos

#### Scenario: Desacoplamento para janela flutuante e retorno seguro
- **WHEN** o usuário aciona `Pop-Out ^` no `BottomDeckHeader` com a aba `Parameters (N)` (ou outra aba do deck) ativa
- **THEN** o painel é transferido para uma janela flutuante independente sem botão `Pop-Out ^` duplicado dentro de `GlobalParametersDeck` quando encaixado
- **AND** acionar `Dock Back` (no botão atualizado do `BottomDeckHeader` ou ao fechar/retornar a janela flutuante) reintegra o painel à aba correspondente no Bottom Deck

#### Scenario: Navegação a partir da barra de transporte
- **WHEN** o usuário clica no pill `Params (N)` localizado na `TransportBar`
- **THEN** o Studio foca e seleciona a aba `Parameters (N)` no Bottom Deck ou traz sua janela flutuante para primeiro plano

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

### Requirement: Alternância de modo de visualização entre Timeline e Layers no palco
O `TimelinePanel` SHALL exibir o `StageModeHeader` (`38 px`) com breadcrumb `< event_path` à esquerda, seletor segmentado centralizado `[ Timeline | Layers ]` e `+ Stem Layer` à direita no modo `Layers` (omitindo `+ Adj. Layer` enquanto não houver regras condicionais no modelo), além de renderizar uma sub-barra exclusiva de `28–30 px` apenas no modo `Timeline` com `Snap`, `Time / Bars`, zoom e `playheadLabel`, mantendo `LIVE PARAMS` visível nos dois modos.

#### Scenario: Modo Timeline preservado como padrão ao abrir evento
- **WHEN** o Studio abre um projeto ou seleciona um evento no modo padrão `Timeline`
- **THEN** o `StageModeHeader` (`38 px`) mantém o seletor `[ Timeline | Layers ]` centralizado com `Timeline` ativo
- **AND** a sub-barra de `28–30 px` logo abaixo do cabeçalho exibe `Snap:`, `Time / Bars`, zoom `- / slider / +` e `playheadLabel` acima do minimapa, da faixa `LIVE PARAMS` e das pistas temporais sem colisão horizontal

#### Scenario: Alternância para o modo Layers exibe a pilha de camadas mantendo a fita de parâmetros
- **WHEN** o usuário aciona a opção `Layers` no seletor segmentado centralizado do palco
- **THEN** a sub-barra de `Snap`/`Time`/`Zoom`, o minimapa e a grade temporal linear são ocultados, exibindo `+ Stem Layer` à direita no `StageModeHeader` e a `LayerStackView` na área central
- **AND** a faixa `LIVE PARAMS` permanece visível acima da pilha para ajuste ao vivo dos parâmetros do evento

### Requirement: Exibição e controle de stems em cards bipartidos na LayerStackView
A `LayerStackView` SHALL exibir cabeçalho de colunas (`CAMADA (STEM & MIX)`, `MÁSCARA DE ATIVAÇÃO (0.0 — 1.0)` e leitura `Cursor ao vivo`), cards `StemLayerCard` de `92 px` com borda inteira `1.5 px` `accentCyan` quando selecionados (barra lateral `4×18 px`, `Eye` e `S` `20×20 px`, badge real `LOOP`/`1-SHOT`, waveform em `bgDark`, barra plana de opacidade sem textbox, `Fade: XX%` real e `StatusBadge`) e `AdjustmentLayerCard` de `46 px` somente para efeitos existentes sem regra booleana fictícia.

#### Scenario: Renderização bipartida de identidade, waveform e opacidade da camada
- **WHEN** um evento com faixas contendo instrumentos de áudio é visualizado no modo `Layers`
- **THEN** cada faixa é renderizada como um card de `92 px` contendo barra de cor `4×18 px`, botão de Olho `20×20`, botão `S` `20×20`, nome, badge real `LOOP` ou `1-SHOT`, miniatura poligonal da waveform em caixa `bgDark` e barra plana `Opacidade XX%` (`0%` a `100%`) sem caixa de texto
- **AND** ajustar a Opacidade atualiza `volume_db` da faixa com registro no histórico de Undo/Redo

#### Scenario: Controle de visibilidade (Olho) e Solo sincronizados com a faixa
- **WHEN** o usuário desativa o botão de Olho ou ativa o botão `S` em um card de camada
- **THEN** o estado de `mute` ou `solo` da faixa correspondente é atualizado imediatamente no modelo do projeto e na audição em tempo real
- **AND** o estado visual do card reflete imediatamente a alteração de forma consistente com o cabeçalho da pista na visão `Timeline`

#### Scenario: Seleção de card na pilha sincroniza o InspectorPanel
- **WHEN** o usuário clica sobre um card de camada na `LayerStackView`
- **THEN** o card selecionado exibe borda `1.5 px` contínua em `accentCyan` ao redor de todo o card
- **AND** a faixa e o respectivo clipe principal são selecionados no `AuthoringModel`, atualizando o `InspectorPanel` (incluindo a seção `MÁSCARA & TRANSIÇÃO`)

### Requirement: Máscara visual de ativação por parâmetro e cursor ao vivo alinhado
A coluna direita de cada card na `LayerStackView` SHALL permitir associar um parâmetro, definir `[min, max]` e `Fade` normalizado (`fade_norm`) via barra horizontal plana (`10 px` sobre `bgDark`) com zona ativa na cor da pista, rampas translúcidas (`fadeTint`) nas extremidades e legenda `Ativa: X – Y` abaixo da barra, mantendo alinhamento horizontal entre camadas e cursor vertical `accentCyan` sincronizado ao valor ao vivo.

#### Scenario: Configuração de faixa ativa e fade gera curva de automação de volume na faixa
- **WHEN** o usuário seleciona um parâmetro na máscara de uma camada e ajusta a faixa ativa `[min, max]` ou o `Fade` na barra plana horizontal
- **THEN** o Studio cria ou atualiza a curva de automação de `volume` vinculada ao parâmetro na respectiva faixa em uma única transação de Undo/Redo, exibindo os blocos de fade translúcido, a zona ativa plana e o texto `Ativa: <min> – <max>`
- **AND** alternar de volta para o modo `Timeline` exibe a curva resultante pronta para edição manual sem perda de dados

#### Scenario: Cursor vertical ao vivo reflete o valor atual do parâmetro em todas as camadas
- **WHEN** o valor de um parâmetro usado pelas máscaras é alterado via `LIVE PARAMS`, `GlobalParametersDeck` ou Live-Linking
- **THEN** o cursor vertical (`2×32 px` `accentCyan`) atravessa a barra de máscara de todas as camadas vinculadas na posição normalizada correspondente e o cabeçalho atualiza `Cursor ao vivo: $<param> = <val>`
- **AND** cada card atualiza seu `StatusBadge` em tempo real (`AGUARDANDO`, `● TOCANDO (XX%)`, `● BASE ATIVA`, `MUTADO (0%)` ou `CURVA MANUAL`)

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

### Requirement: Navegação por abas, ações contextuais e lista fixa de parâmetros no AuthoringTreeBrowser
O `AuthoringTreeBrowser` SHALL exibir o cabeçalho `DockHeader` (`24 px`, `BROWSER` + contagem real como `1 Bank`), a barra `BrowserTabs` (`34 px`) com `[ Events | Snapshots | Buses | Assets ]` (abrigando `VCAs` dentro de `Buses`), campo `SearchBox` (`bgDark`) sem botões `+`/`Dup`/`Del` na barra, ações via menu de contexto e atalhos, e seção fixa inferior `PROJECT PARAMETERS (N)` com seleção sincronizada ao `InspectorPanel`.

#### Scenario: Alternância entre as quatro abas do navegador e agrupamento de VCAs em Buses
- **WHEN** o usuário seleciona cada uma das abas `Events`, `Snapshots`, `Buses` e `Assets` na barra `BrowserTabs`
- **THEN** a árvore exibe apenas os itens da categoria selecionada com destaque de seleção em `tintCyan` e borda `accentCyan`
- **AND** a aba `Buses` lista os barramentos hierárquicos e uma sub-seção dedicada para `VCAs`

#### Scenario: Criação, renomeação, duplicação e exclusão via menu de contexto e atalhos
- **WHEN** o usuário abre o menu de contexto com o botão direito sobre um item ou área da aba ativa no `AuthoringTreeBrowser`, ou pressiona `F2`, `Ctrl+D` ou `Del`
- **THEN** as ações equivalentes de criação (`New Event`, `New Snapshot`, `New Bus`, `New VCA`, `Import Audio...`), `Rename (F2)`, `Duplicate (Ctrl+D)` e `Delete (Del)` executam com suporte a Undo/Redo sem botões `+ / Dup / Del` visíveis na barra de busca

#### Scenario: Seção fixa inferior PROJECT PARAMETERS (N)
- **WHEN** o projeto possui parâmetros globais cadastrados
- **THEN** a seção fixa `PROJECT PARAMETERS (N)` abaixo da árvore lista cada parâmetro global em uma linha `bgDark` (`22 px`) exibindo nome em `accentCyan` e domínio/tipo real em `textMuted`
- **AND** clicar em um parâmetro seleciona `ItemType::GlobalParameter` no `InspectorPanel`, enquanto o clique direito oferece `New Global Parameter`, `Rename`, `Duplicate` e `Delete`
