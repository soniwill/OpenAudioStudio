# Proposal

## Why

O OpenAudio Studio já permite autorizar, inspecionar e compilar em bancos binários instrumentos dos tipos `EVENT` (eventos aninhados/referenciados) e `COMMAND` (disparo de eventos e snapshots). No entanto, o motor de áudio em tempo real (`native/src/core/event_instance.cpp`) atualmente ignora esses tipos durante a reprodução da timeline e na avaliação de parâmetros, impossibilitando a composição de áudio hierárquico e lógica de disparo sem intervenção de código de jogo. Esta change fecha a lacuna de execução em tempo real de `EVENT` e `COMMAND`, estabelecendo o primeiro spec canônico da capacidade de conceito `instruments` alinhado ao FMOD Studio 2.04.

## What Changes

- **Novo Spec Canônico de Conceito (`instruments`)**: Migra e formaliza os requisitos de Single, Multi, Programmer, Silence, Event e Command Instruments, estruturados com os prefixos normativos `Runtime —`, `Autoria —` e `Unity —`.
- **Execução de Event Instruments em Runtime**: Quando o cursor de reprodução atingir um Event Instrument, o motor MUST instanciar o evento alvo e mixar seu fluxo de áudio diretamente na pista (track) do evento pai, herdando ganho composto, efeitos e automações da pista, e sincronizando o ciclo de vida (interrupção do pai encerra o evento filho com fadeout).
- **Execução de Command Instruments em Runtime**: Quando o cursor atingir um Command Instrument, o motor MUST executar a ação configurada de forma determinística e atômica na game thread / update loop:
  - `start_event`: dispara uma nova instância do evento alvo.
  - `stop_event`: interrompe instâncias ativas do evento alvo (imediato ou com fadeout).
  - `snapshot_on`: ativa o snapshot alvo no mixer.
  - `snapshot_off`: desativa o snapshot alvo no mixer.
- **Integração de Avaliação na Timeline**: Atualiza os loops de varredura de instrumentos em `EventInstance` para avaliar instrumentos do tipo `Event` e `Command` durante avanço da timeline e saltos de loop.

## Capabilities

### New Capabilities
- `instruments`: Comportamento de ponta a ponta dos instrumentos de áudio e lógica (Single, Multi, Programmer, Silence, Event e Command) em tempo real, na interface de autoria do Studio e na integração com a Unity.

### Modified Capabilities

## Impact

- **Motor Nativo (`native/src/core/event_instance.cpp`, `event_instance.h`)**: Suporte a gerenciamento de instâncias filhas de eventos e emissão de comandos de sistema/snapshot no loop de áudio/atualização.
- **Testes CTest (`native/tests/studio_events.cpp`)**: Novos casos de teste canônicos exercitando disparo de Event Instrument aninhado e comandos de Start/Stop e SnapshotOn/Off.
- **Documentação e Rastreabilidade (`docs/specs/README.md`)**: Atualiza o status da capacidade `instruments` no mapa legado para migrado canonicamente.
- **Out of Scope (Fase 2)**: Scatterer Instrument e modulação dinâmica de parâmetros em tempo real para spawners estocásticos serão abordados na change seguinte.
