# Spec Delta: studio-shell

## MODIFIED Requirements

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

### Requirement: Cabeçalhos de docking em grafite
O Studio SHALL usar cabeçalhos únicos `DockHeader` (`24 px`, `dockHeaderBg`) com título em caixa alta e metadados à direita nos painéis isolados (sem cabeçalho duplo), e `DockTab`s (`22 px`, `rounded=3 px`) nos grupos de abas e `BottomDeckHeader` (`34 px`) destacando a aba ativa com `dockTabActiveBg`, borda `accentCyan` e foco de teclado visível.

#### Scenario: Distinguir título, seleção e foco
- **WHEN** o usuário compara um dock com painel único (`BROWSER`, `STAGE`, `PROPERTY INSPECTOR`) com uma barra de abas (`BrowserTabs`, `BottomDeckHeader` ou grupo multi-aba) e navega pelo teclado
- **THEN** o painel único MUST exibir um único cabeçalho grafite (`24 px`) com título neutro à esquerda e metadado/controle auxiliar à direita sem barra duplicada
- **AND** em grupos de abas, a aba ativa MUST destacar-se das inativas com borda `accentCyan` e contraste de fundo/texto mantendo o foco de teclado perceptível independentemente da seleção

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
O `ContextualBottomDeck` SHALL exibir um cabeçalho único `BottomDeckHeader` (`34 px`) com as 7 abas na ordem `Mixer`, `Effect Chain`, `Logic Conditions`, `Playlist`, `3D Spatializer`, `Parameters (N)` e `Diagnostics`, além do botão único `Pop-Out ^` (ausente dentro de `GlobalParametersDeck` quando encaixado). A aba `Parameters (N)` SHALL conter barra de filtros (`All (N)`, `• Used in Event (N)`, `Modified (N)`, busca e `Reset All`) e cards `~131 px` (`cardBodyBg`) com controles especializados e rodapé derivável real.

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

## ADDED Requirements

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
