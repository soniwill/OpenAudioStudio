# Spec Delta — studio-shell

## ADDED Requirements

### Requirement: Inicialização headless e isolamento de ambiente no MainComponent
O `MainComponent` SHALL aceitar opções de inicialização para forçar `StudioEngine::init_headless()`, isolar o arquivo `oae_studio_layout.json`, desabilitar o carregamento automático de projeto de exemplo, desabilitar o servidor TCP do Live-Linking e controlar timers periódicos, mantendo intacto o comportamento padrão do construtor desktop.

#### Scenario: Inicialização em modo headless de áudio com layout isolado
- **WHEN** `MainComponent` é instanciado com modo de áudio headless e caminho de layout isolado
- **THEN** a engine inicializa sem dispositivo físico de áudio e aceita renderização PCM determinística em memória
- **AND** o arquivo `oae_studio_layout.json` existente no diretório de execução desktop não é lido nem alterado

#### Scenario: Compatibilidade integral do construtor padrão na execução desktop
- **WHEN** `MainComponent` é instanciado pelo construtor padrão da aplicação desktop
- **THEN** o Studio preserva o carregamento de layout do usuário, a inicialização normal de áudio e os timers padrão

### Requirement: Identidade semântica única (ComponentID) para controles interativos da UI
Todo controle interativo visível sob `MainComponent` e sob janelas flutuantes destacadas (`OaeFloatingPanelWindow`) MUST possuir um `ComponentID` semântico não vazio e único entre elementos visíveis simultaneamente, preservando os `ComponentID`s já documentados para automação MCP.

#### Scenario: Preenchimento de ComponentIDs ausentes e desambiguação de colisões simultâneas
- **WHEN** a interface exibe controles do Browser, Timeline, cabeçalhos e raias de pista, fita de parâmetros, Layers, Mixer, Playlist, Parâmetros Globais, Diagnostics e cards ou editores do Inspector
- **THEN** todo controle interativo possui `ComponentID` determinístico sem colisões entre elementos visíveis simultaneamente
- **AND** todos os identificadores existentes documentados para automação MCP permanecem compatíveis

### Requirement: Exposição de alvos interativos locais em widgets customizados
Widgets customizados com hit-testing manual SHALL implementar uma interface somente leitura expondo seus alvos clicáveis ou arrastáveis em coordenadas locais para inspeção automatizada sem alterar a renderização ou interação normal do usuário.

#### Scenario: Consulta de alvos locais em widgets customizados
- **WHEN** a suíte de inspeção consulta um widget customizado interativo (`ClipComponent`, `AutomationLaneComponent`, `TimelineRuler`, `TimelineOverviewMinimap`, `MaskRangeCanvas`, `EqPlot`, `Spatializer3DDeck`, `AhdsrCurveEditor`, `AutomationCurveEditor`, `ParametersListComponent`, `TrackHeaderItem` ou `GripHandle`)
- **THEN** o widget retorna retângulos locais válidos e coerentes com seus alvos pintados sem modificar sua aparência normal

### Requirement: Interceptação não-bloqueante de diálogos nativos e menus síncronos em modo de teste
Ações da interface que invocam `FileChooser`, `AlertWindow` ou `PopupMenu` síncrono SHALL consultar um gancho de interceptação quando executado em modo de teste, retornando resposta determinística sem bloquear a thread principal e mantendo o comportamento nativo fora do modo de teste.

#### Scenario: Interceptação de diálogos modais e menus síncronos em modo de teste
- **WHEN** em modo de teste um controle aciona `FileChooser`, `AlertWindow` ou `PopupMenu` síncrono (como criação/abertura de projeto, importação/renomeação/exclusão no Browser, salvar preset ou adicionar efeito no `BusCard`)
- **THEN** o gancho intercepta a chamada com resposta simulada sem abrir janelas modais do sistema operacional
- **AND** fora do modo de teste o comportamento interativo nativo permanece inalterado

### Requirement: Auditoria automática Zero Controle Órfão e checagem física de alcançabilidade
O Studio SHALL fornecer uma auditoria automática que percorre `MainComponent` e janelas `OaeFloatingPanelWindow` por uma matriz canônica de contextos e MUST falhar diante de qualquer controle interativo sem `ComponentID` único, com geometria nula ou recortada, ocluído no `hitTest` real ou sem contrato de teste registrado.

#### Scenario: Cobertura da matriz canônica de contextos
- **WHEN** a auditoria percorre a matriz canônica de contextos
- **THEN** ela inspeciona todas as seleções de entidades e os seis tipos de instrumento, os modos `Timeline` e `Layers`, todas as abas do Browser, todas as 7 abas do Bottom Deck e uma janela destacada (*pop-out*)

#### Scenario: Falha determinística ao encontrar controle sem ID ou com ID duplicado
- **WHEN** o auditor encontra um controle interativo visível sem `ComponentID` ou com `ComponentID` repetido simultaneamente na árvore
- **THEN** a auditoria falha reportando o código `MISSING_ID` ou `DUPLICATE_ID` com o caminho do componente e encerra com código de saída diferente de zero

#### Scenario: Falha determinística ao encontrar controle com geometria inválida ou ocluído
- **WHEN** o auditor detecta um controle interativo com largura ou altura não positiva, recortado fora do ancestral ou interceptado por outro componente no `hitTest`
- **THEN** a auditoria falha reportando `ZERO_BOUNDS`, `CLIPPED` ou `OCCLUDED_HITTEST` e encerra com código de saída diferente de zero

#### Scenario: Falha determinística ao encontrar controle interativo sem contrato de teste
- **WHEN** um controle interativo novo ou existente aparece na matriz canônica de contextos sem contrato de teste associado
- **THEN** a auditoria falha com código `UNCOVERED` identificando o `ComponentID` descoberto e impede falsos positivos de cobertura

### Requirement: Driver de interação realista e modo CLI headless (--test-ui-controls)
O executável do Studio SHALL suportar o modo `--test-ui-controls` (com `--filter=<padrao>`, `--audit-only` e `--report=<arquivo.json>`) em um único processo headless sobre fixture sintético, interagindo exclusivamente por um driver que valida visibilidade, habilitação, bounds e `hitTest` antes de sintetizar eventos de entrada.

#### Scenario: Bloqueio de falso positivo no pre-flight do driver
- **WHEN** o driver tenta acionar um controle oculto, desabilitado, sem área ou coberto por outro componente
- **THEN** o driver recusa o acionamento antes de sintetizar eventos de entrada e falha com diagnóstico explícito

#### Scenario: Execução filtrada rápida e geração de relatório JSON
- **WHEN** `--test-ui-controls` é executado com `--filter=<padrao>` e `--report=<arquivo.json>`
- **THEN** apenas os contratos correspondentes ao padrão são executados sem falhar pelos painéis não filtrados e o relatório JSON registra totais de aprovação, falhas, controles órfãos e tempo decorrido

### Requirement: Verificação ponta a ponta de autoria, visualização e efeito acústico real
A suíte `--test-ui-controls` SHALL verificar ponta a ponta as mutações de `AuthoringModel` com `Undo`/`Redo` e round-trip de UI para controles de autoria, as mudanças observáveis de shell para controles de visualização, e a variação acústica real em render PCM headless (`StudioEngine::update()` e `render()`) para controles que afetam áudio.

#### Scenario: Verificação de controles de autoria com round-trip na UI e Undo/Redo
- **WHEN** a suíte aciona controles de autoria na Timeline, LayerStack, Inspector, Playlist ou Logic Conditions via driver realista
- **THEN** o estado no `AuthoringModel` muda conforme esperado, sobrevive ao `refresh()` da UI e é revertido e reaplicado por `Undo`/`Redo`

#### Scenario: Verificação acústica ponta a ponta para transporte, mixagem e controles de pista
- **WHEN** a suíte interage com os controles reais da UI de transporte (`btn_play`, `btn_pause`, `btn_stop`, `btn_loop`, `btn_key_off`), Mixer e `TrackHeaderPanel` (faders, pan, mute, solo) durante audição headless
- **THEN** a renderização PCM em memória comprova sinal audível após Play, silêncio após Stop ou Mute, atenuação mensurável em dB ao reduzir fader, isolamento de faixa ao acionar Solo e deslocamento mensurável da energia L/R ao mover Pan

#### Scenario: Verificação acústica ponta a ponta para DSP, espacialização 3D e parâmetros ao vivo
- **WHEN** a suíte interage com os controles de `EffectChainDeck`/`EqPlot`, `Spatializer3DDeck`, `TimelineParameterStrip`, `GlobalParametersDeck` e `MaskRangeCanvas`
- **THEN** a saída renderizada reflete a resposta acústica esperada (atenuação espectral de filtro e restauração por bypass, atenuação e balanço 3D por distância/posição e variação de ganho ou ativação de camadas por parâmetro)

### Requirement: Cobertura de controles em janelas flutuantes via TopLevelWindow
Quando um painel ou aba do Bottom Deck é destacado para uma janela flutuante (`OaeFloatingPanelWindow`), o auditor e o driver de interação SHALL descobrir e exercitar os controles hospedados na janela flutuante por meio de `juce::TopLevelWindow`, validando também o retorno ao dock principal.

#### Scenario: Ciclo completo de pop-out, interação em janela flutuante e dock-back
- **WHEN** a suíte aciona o controle de destaque (*pop-out*) de um painel ou deck em modo headless
- **THEN** a janela flutuante é encontrada via `TopLevelWindow` com seus controles auditados e acionáveis
- **AND** acionar `btn_floating_dock_back` reacopla o painel ao `MainComponent` preservando sua funcionalidade e alcançabilidade
