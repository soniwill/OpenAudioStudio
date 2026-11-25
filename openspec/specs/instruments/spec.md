# instruments Specification

## Purpose

Define a especificação normativa de instrumentos de áudio e de lógica (Single, Multi, Programmer, Silence, Event e Command), cobrindo execução em runtime, autoria na DAW e integração em engine de jogo.

## Requirements

### Requirement: Runtime — Disparo e agendamento de Single Instruments
O motor de áudio MUST disparar e reproduzir o sample de áudio associado a um Single Instrument com precisão de frame quando o cursor de reprodução atingir seu offset na timeline ou quando sua condição de parâmetro for atendida.

#### Scenario: Disparo na timeline com offset e volume base
- **WHEN** uma instância de evento reproduzir uma timeline atingindo o offset de um Single Instrument
- **THEN** o motor MUST alocar ou reutilizar um canal de voz renderizando o sample correspondente com o ganho e pitch especificados no instrumento

### Requirement: Runtime — Seleção de itens em Multi Instruments
O motor de áudio MUST sortear ou iterar entradas de playlist de um Multi Instrument em conformidade com o modo de playlist configurado (Random, RandomNoRepeat, Sequential, Shuffle) e escopo de seleção (Global ou Instance). Em modos aleatórios, sorteios de plays distintos de um mesmo evento MUST ser independentes, sem semente fixa por play, e MUST convergir à distribuição de pesos configurada.

#### Scenario: Seleção determinística por peso
- **WHEN** um Multi Instrument for disparado
- **THEN** a entrada da playlist selecionada MUST convergir estritamente para a distribuição de pesos configurada utilizando o gerador pseudoaleatório PCG32

#### Scenario: Variação de atraso estocástico
- **WHEN** um Multi Instrument possuir `delay_ms_random_range` maior que zero
- **THEN** o disparo da voz MUST ser atrasado deterministicamente por um valor aleatório entre zero e o limite máximo configurado

#### Scenario: Sorteios independentes entre plays distintos
- **WHEN** o mesmo evento com Multi Instrument `Random (weighted)` é reproduzido múltiplas vezes a partir de plays independentes
- **THEN** a primeira escolha de cada play MUST variar entre plays, sem repetição idêntica sistemática imposta por semente fixa
- **AND** o conjunto de escolhas observadas MUST conter mais de uma entrada da playlist quando ambas têm peso não nulo

### Requirement: Runtime — Execução de Programmer Instruments
O motor de áudio MUST solicitar o asset de áudio dinamicamente via callback de evento para a aplicação ou resolver a entrada correspondente na tabela de áudio interna através da chave configurada.

#### Scenario: Resolução via callback de usuário
- **WHEN** o cursor atingir um Programmer Instrument com callback registrado na instância
- **THEN** o motor MUST emitir `OAE_EVENT_CALLBACK_CREATE_PROGRAMMER_SOUND` e reproduzir o handle retornado

### Requirement: Runtime — Execução e sub-mixagem de Event Instruments
O motor de áudio MUST instanciar o evento referenciado por um Event Instrument no momento do disparo e rotear o sinal de áudio gerado diretamente para a pista (track) do evento pai, herdando ganho, pan, efeitos e automações da pista.

#### Scenario: Disparo de evento aninhado na timeline
- **WHEN** a timeline do evento pai atingir o offset de um Event Instrument
- **THEN** o motor MUST criar uma instância ativa do evento alvo e somar suas saídas no sub-mixer da pista onde o instrumento reside

#### Scenario: Encerramento do evento pai com instâncias filhas ativas
- **WHEN** a instância do evento pai for interrompida via `stop()`
- **THEN** todas as instâncias filhas disparadas por Event Instruments MUST ser interrompidas respeitando suas caudas de fadeout

### Requirement: Runtime — Despacho atômico de Command Instruments
O motor de áudio MUST executar a ação de controle especificada por um Command Instrument (disparo ou parada de eventos e ativação ou desativação de snapshots) de maneira atômica e segura no ciclo de atualização da game thread.

#### Scenario: Comando de disparo de evento alvo
- **WHEN** o cursor de reprodução atingir um Command Instrument configurado como `start_event`
- **THEN** o motor MUST criar e disparar imediatamente uma nova instância independente do evento alvo

#### Scenario: Comando de ativação de snapshot
- **WHEN** o cursor de reprodução atingir um Command Instrument configurado como `snapshot_on`
- **THEN** o motor MUST ativar o snapshot referenciado com o fade-in configurado no mixer

### Requirement: Autoria — Criação e parametrização de instrumentos no Studio
A interface de autoria do OpenAudio Studio MUST permitir criar, posicionar na timeline, inspecionar propriedades acústicas e selecionar alvos válidos para todos os tipos suportados de instrumentos, rejeitando referências cíclicas em eventos e comandos.

#### Scenario: Adição de Event Instrument com alvo válido
- **WHEN** o usuário adicionar um Event Instrument a uma pista no Inspetor
- **THEN** o Studio MUST exibir apenas eventos válidos do projeto que não criem dependência cíclica direta ou indireta

### Requirement: Unity — Disparo e controle de emissores com instrumentos de evento
O pacote Unity com.openaudio.core MUST fornecer integração completa com eventos que contenham qualquer tipo de instrumento suportado, garantindo que emissores 2D e 3D gerenciem o ciclo de vida sem vazamento de handles nativos.

#### Scenario: Parada de emissor com evento contendo Event Instruments
- **WHEN** um componente `StudioEventEmitter` for desativado ou destruído
- **THEN** o evento principal e quaisquer instâncias filhas ativas MUST ser liberados sem emitir erros de handle inválido

### Requirement: Runtime — Agendamento estocástico e polifonia de Scatterer Instruments
Enquanto um `Scatterer Instrument` estiver ativo na timeline ou sob condição válida, o motor de áudio MUST disparar grãos da sua playlist em intervalos sorteados deterministicamente (via `PCG32`) entre `spawn_interval_min_ms` e `spawn_interval_max_ms`, respeitando o limite simultâneo `polyphony` e o teto opcional `total_sounds` (`0` = contínuo enquanto ativo).

#### Scenario: Disparo periódico respeitando intervalo e polifonia
- **WHEN** um evento contendo um `Scatterer Instrument` com `polyphony = 2` e `spawn_interval_min_ms = 50`, `spawn_interval_max_ms = 100` permanece ativo em `EventInstance::update()`
- **THEN** o motor dispara novos grãos respeitando a janela de intervalo configurada
- **AND** nunca mantém mais de 2 grãos ativos simultaneamente para aquele spawner

#### Scenario: Encerramento do Scatterer ao sair da região ou parar evento
- **WHEN** o cursor de timeline sai da região de um `Scatterer Instrument` síncrono ou o evento pai recebe `stop()`
- **THEN** o spawner cessa novos disparos imediatamente e propaga a parada aos grãos ativos conforme o modo de parada

### Requirement: Runtime — Dispersão espacial e variação por grão em Scatterer Instruments
Para cada grão disparado por um `Scatterer Instrument`, o motor de áudio MUST aplicar as variações configuradas de volume/pitch e calcular uma distância aleatória em `[min_scatter_distance, max_scatter_distance]` e ângulo azimutal via `PCG32`, aplicando panning estéreo e atenuação proporcional como fallback 2D quando o evento não possuir posicionamento 3D completo.

#### Scenario: Grãos recebem dispersão estéreo e atenuação determinística
- **WHEN** um `Scatterer Instrument` dispara múltiplos grãos com `max_scatter_distance > min_scatter_distance`
- **THEN** cada grão recebe ganho atenuado proporcionalmente à distância sorteada e deslocamento panorâmico distribuído entre esquerda e direita

### Requirement: Autoria — Compilação e persistência de Programmer Instrument e tabela de áudio
O compilador de projetos (`oae_bankc`) e o formato binário `.bank` MUST suportar instrumentos do tipo `programmer` com `programmer_key` e a tabela de áudio (`audio_table`) mapeando chaves para `sample_id`, preservando compatibilidade com bancos v1 existentes que não declaram tabela de áudio. Toda via de carregamento nativo de banco — incluindo a carga síncrona pela API C — MUST popular a `audio_table` no `ModelSet` reconstruído.

#### Scenario: Compilação e carregamento de Programmer Instrument com audio_table
- **WHEN** um projeto JSON declara uma entrada em `audio_table` associando uma chave a um `sample_id` válido e um evento com instrumento `kind: "programmer"` referenciando essa `programmer_key`
- **THEN** o compilador `oae_bankc` gera o `.bank` sem erro e o carregador nativo popula `programmer_key` e `AudioTableEntry` no `ModelSet`
- **AND** o runtime reproduz o sample associado quando não há callback de usuário registrado

#### Scenario: Carga síncrona pela API C preserva a audio_table
- **WHEN** um banco `.bank` com `audio_table` é carregado pelo caminho síncrono da API C que reconstrói o `ModelSet` a partir do binário
- **THEN** a tabela de áudio do banco MUST estar presente no `ModelSet` reconstruído com todas as entradas chave → `sample_id`
- **AND** um Programmer Instrument sem callback de usuário que referencia uma chave da tabela MUST reproduzir o sample correspondente na prévia

#### Scenario: Rejeição de entrada inválida na audio_table durante compilação
- **WHEN** uma entrada de `audio_table` possui chave vazia, chave duplicada ou referencia um `sample_id` inexistente
- **THEN** o compilador `oae_bankc` falha com erro explícito indicando o caminho da entrada inválida

### Requirement: Autoria — Entradas target_event em playlists de Multi e Scatterer
O `AuthoringModel` e a validação de projeto MUST suportar entradas de playlist com `target_event` (além de `sample_id`) em instrumentos `multi` e `scatterer`, rejeitando referências que criem ciclos diretos ou indiretos entre eventos.

#### Scenario: Validação anti-ciclo em playlist mista
- **WHEN** um usuário ou projeto tenta adicionar `target_event` na playlist de um `Multi` ou `Scatterer`
- **THEN** apenas eventos que não formam ciclo direto ou indireto com o evento pai são aceitos como alvos válidos

### Requirement: Autoria — Suporte à propriedade async em project.json e oae_bankc
O schema de projeto (`project-v1.schema.proposed.json`), o `AuthoringModel` e o compilador `oae_bankc` MUST aceitar a propriedade booleana opcional `async` (padrão `false`) em instrumentos suportados (`single`, `multi`, `scatterer`, `programmer`, `event`), preservando compatibilidade com projetos existentes que omitem o campo e compilando `async = true` para o banco `.bank`.

#### Scenario: Compilação de instrumento com async habilitado
- **WHEN** um `project.json` declara `"async": true` em um instrumento `single` (como no evento `event:/sfx/charge_sustain_async`) e é compilado pelo `oae_bankc`
- **THEN** o compilador aceita o campo sem erro de validação e grava `async = true` no `.bank` gerado

#### Scenario: Compatibilidade retroativa quando async é omitido
- **WHEN** um `project.json` omite a propriedade `async` em um instrumento
- **THEN** o compilador e o `AuthoringModel` assumem `async = false` sem quebrar projetos existentes

#### Scenario: Continuidade de instrumento assíncrono durante SustainPoint
- **WHEN** um evento possui um instrumento com `async = true` e `loop = true` iniciado antes de um `SustainPoint`
- **THEN** o cursor da timeline congela ao atingir o `SustainPoint`, mas a voz assíncrona continua reproduzindo até receber `key_off()`

### Requirement: Runtime — Sound Stealing Oldest na polifonia do Scatterer Instrument
Quando um `Scatterer Instrument` precisar disparar um novo grão e a contagem de grãos ativos simultaneamente atingir `polyphony` (`polyphony > 0`), o motor de áudio MUST aplicar *Sound Stealing* (`Oldest`), interrompendo o grão ativo mais antigo daquele spawner para liberar uma vaga antes do novo disparo.

#### Scenario: Roubo do grão mais antigo quando polifonia máxima é atingida
- **WHEN** um `Scatterer Instrument` com `polyphony = 2` já mantém 2 grãos ativos e o temporizador solicita o disparo de um terceiro grão
- **THEN** o grão mais antigo em execução daquele spawner é interrompido imediatamente
- **AND** o terceiro grão é disparado preservando o teto simultâneo de 2 grãos ativos

### Requirement: Autoria — Disparo de Instrumentos por Intervalo de Parameter Sheet
O schema `project.json` v1, o compilador `oae_bankc`, o formato `.bank` e a DAW (`AuthoringModel` e `InspectorPanel`) MUST suportar vincular instrumentos a um `sheet_parameter` e intervalo `[param_min, param_max]`, preservando o disparo e liberação em runtime via `evaluate_parameter_sheets`.

#### Scenario: Compilação e disparo por Parameter Sheet em banco compilado
- **WHEN** um instrumento declara `sheet_parameter`, `param_min` e `param_max` dentro do domínio do parâmetro referenciado e o projeto é compilado por `oae_bankc`
- **THEN** o banco `.bank` gerado restaura `sheet_parameter`, `param_min` e `param_max` no `ModelSet`
- **AND** o evento reproduz o instrumento apenas quando o parâmetro entra no intervalo configurado

#### Scenario: Rejeição de Parameter Sheet com parâmetro inexistente ou intervalo inválido
- **WHEN** um instrumento referencia um `sheet_parameter` inexistente ou declara `param_min > param_max` fora do domínio `[min, max]` do parâmetro
- **THEN** o compilador `oae_bankc` rejeita o projeto com diagnóstico explícito
