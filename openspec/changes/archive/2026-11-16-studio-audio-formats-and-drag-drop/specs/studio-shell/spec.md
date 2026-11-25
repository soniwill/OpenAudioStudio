# Spec Delta

## ADDED Requirements

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
O Studio SHALL disponibilizar a opção "Import Audio..." no menu de contexto do nó de Audio Assets no navegador do projeto, permitindo selecionar um ou mais arquivos de áudio suportados através da caixa de diálogo do sistema operacional.

#### Scenario: Seleção e importação de áudio via diálogo nativo
- **WHEN** o usuário aciona a ação "Import Audio..." no menu de contexto de Audio Assets e confirma a seleção de arquivos no diálogo
- **THEN** os arquivos selecionados são copiados para `assets/` e registrados como samples no projeto com atualização imediata da interface
