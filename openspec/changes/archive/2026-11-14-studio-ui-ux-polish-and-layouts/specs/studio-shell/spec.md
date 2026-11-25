# Spec Delta — studio-shell

## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: Rastreamento 1:1 do cursor no thumb de sliders lineares
Os sliders lineares do OpenAudio Studio SHALL mover o indicador visual (bolinha/thumb) em proporção `1:1` com o deslocamento do ponteiro sobre a extensão física útil do trilho durante o arraste normal, mantendo a geometria do thumb alinhada ao trilho e aplicando o fator fino (`0.1x`) apenas enquanto `Shift` estiver pressionado.

#### Scenario: Arraste horizontal sem atraso em slider linear
- **WHEN** o usuário clica sobre o thumb ou trilho de um slider linear horizontal e arrasta o ponteiro por `N` pixels dentro da extensão útil do trilho sem pressionar `Shift`
- **THEN** o centro do thumb acompanha exatamente a posição horizontal percorrida pelo ponteiro sem salto inicial e sem depender de sensibilidade fixa de 250 pixels

#### Scenario: Preservação do ajuste fino com Shift pressionado
- **WHEN** o usuário mantém a tecla `Shift` pressionada durante o arraste de um slider linear
- **THEN** o deslocamento do valor passa a aplicar o fator fino de `0.1x` em relação ao movimento do ponteiro a partir da posição atual do thumb

### Requirement: Agrupamento modular em cards colapsáveis por escopo (Clip, Track e Event) no InspectorPanel
O `InspectorPanel` SHALL organizar os parâmetros de autoria em três cards colapsáveis e visualmente distintos por escopo — **Clip**, **Track** e **Event** — exibindo apenas a pista e o clipe atualmente selecionados, com subgrupos lógicos internos e identificação visual pela cor da pista e badge do tipo.

#### Scenario: Seleção de clipe exibe cards agrupados em ordem contextual
- **WHEN** o usuário seleciona um clipe em uma pista no palco ou no navegador
- **THEN** o `InspectorPanel` apresenta o card colapsável **Clip** no topo (com subgrupos lógicos de fonte/disparo, fades/tempo e modulação do clipe), seguido pelo card **Track** da pista selecionada e pelo card **Event** na base
- **AND** outras pistas e clipes não selecionados do mesmo evento não são listados simultaneamente no painel

#### Scenario: Alternância e colapso independente de cards no Inspector
- **WHEN** o usuário recolhe ou expande o cabeçalho de qualquer card (`Clip`, `Track` ou `Event`) no `InspectorPanel`
- **THEN** o conteúdo daquele card é recolhido ou expandido preservando o estado dos demais cards e mantendo clara a separação visual entre propriedades do clipe, da pista e do evento

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
O sistema de docking do Studio SHALL permitir reconfigurar o slot de cada painel, fechar ou ocultar painéis individualmente ou todos simultaneamente mantendo uma barra mínima de restauração, alternar entre três templates nativos (`Default (Authoring)`, `Stage Focus` e `Mixing & Mastering`), criar e excluir layouts personalizados e restaurar o layout padrão via `Reset Layout`.

#### Scenario: Ocultação individual e ocultação total de painéis com restauração imediata
- **WHEN** o usuário fecha ou oculta um painel específico ou aciona a ação de ocultar todos os painéis na barra de workspace
- **THEN** os painéis ocultos deixam de ocupar área de tela, os painéis visíveis expandem-se para preencher o espaço livre e a barra superior compacta permanece acessível para reabrir qualquer painel com um clique

#### Scenario: Aplicação de templates nativos, criação, exclusão e reset de layouts
- **WHEN** o usuário seleciona um dos três templates nativos, salva a configuração atual como um novo layout personalizado, exclui um layout personalizado ou aciona `Reset Layout`
- **THEN** o arranjo, a visibilidade, os slots dos painéis e as dimensões dos divisores são aplicados imediatamente, persistidos em `oae_studio_layout.json` e os templates nativos permanecem protegidos contra exclusão
