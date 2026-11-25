# Spec Delta

## ADDED Requirements

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

## MODIFIED Requirements

### Requirement: Cobertura profunda e exemplos de uso do Bottom Deck no manual offline

O manual offline do OpenAudio Studio SHALL detalhar a teoria acústica, a motivação de design, os parâmetros operacionais e os exemplos práticos de Mixer, Effects, Playlist, Logic, Spatializer e Diagnostics, incluindo capturas representativas. Diagnostics SHALL ser descrito como aba quando embutido e como painel encaixável quando destacado do Bottom Deck.

#### Scenario: Consulta teórica aos processadores DSP e medição

- **WHEN** o usuário acessa a documentação do Mixer Deck ou do Effects Rack no capítulo 7
- **THEN** o manual apresenta a motivação acústica e as faixas de operação dos sete efeitos DSP nativos, a balística de medição Peak/RMS e K-System e as práticas de headroom para evitar clipping na soma do Master.

#### Scenario: Consulta à estocástica e lógica adaptativa

- **WHEN** o usuário consulta Playlist / Weights ou Logic & Triggers
- **THEN** o manual detalha seleção estocástica, pesos, marcadores de transição, quantização e regiões de loop
- **AND** os exemplos mostram uma playlist com sample e evento e uma transição com marcador de destino, condição e resultado esperado.

#### Scenario: Consulta à espacialização 3D e telemetria

- **WHEN** o usuário consulta 3D Spatializer ou Diagnostics & Log
- **THEN** o manual explica as curvas de distância e o envelopment da prévia local, a configuração de cones do emissor Unity e como interpretar as mensagens de prévia, compilação e conexão
- **AND** a absorção atmosférica aparece como conceito acústico e a contagem de vozes é localizada na ferramenta que a exibe, como o overlay da Unity.

#### Scenario: Consultar logs enquanto alterna o Bottom Deck

- **WHEN** o usuário segue o procedimento para montar o layout com Inspector lateral e Diagnostics & Log abaixo do Bottom Deck
- **THEN** encontra o painel pelo controle de workspace, consegue encaixá-lo em uma divisão independente e mantém as mensagens visíveis ao alternar as abas internas do Deck
- **AND** o manual mostra como reabrir painéis ocultos e restaurar ou salvar o layout.

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
