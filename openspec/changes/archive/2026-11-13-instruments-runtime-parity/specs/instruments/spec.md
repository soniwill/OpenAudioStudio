# Spec Delta

## Purpose

Define a especificação normativa de instrumentos de áudio e de lógica (Single, Multi, Programmer, Silence, Event e Command), cobrindo execução em runtime, autoria na DAW e integração em engine de jogo.

## ADDED Requirements

### Requirement: Runtime — Disparo e agendamento de Single Instruments
O motor de áudio MUST disparar e reproduzir o sample de áudio associado a um Single Instrument com precisão de frame quando o cursor de reprodução atingir seu offset na timeline ou quando sua condição de parâmetro for atendida.

#### Scenario: Disparo na timeline com offset e volume base
- **WHEN** uma instância de evento reproduzir uma timeline atingindo o offset de um Single Instrument
- **THEN** o motor MUST alocar ou reutilizar um canal de voz renderizando o sample correspondente com o ganho e pitch especificados no instrumento

### Requirement: Runtime — Seleção de itens em Multi Instruments
O motor de áudio MUST sortear ou iterar entradas de playlist de um Multi Instrument em conformidade com o modo de playlist configurado (Random, RandomNoRepeat, Sequential, Shuffle) e escopo de seleção (Global ou Instance).

#### Scenario: Seleção determinística por peso
- **WHEN** um Multi Instrument for disparado
- **THEN** a entrada da playlist selecionada MUST convergir estritamente para a distribuição de pesos configurada utilizando o gerador pseudoaleatório PCG32

#### Scenario: Variação de atraso estocástico
- **WHEN** um Multi Instrument possuir `delay_ms_random_range` maior que zero
- **THEN** o disparo da voz MUST ser atrasado deterministicamente por um valor aleatório entre zero e o limite máximo configurado

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
