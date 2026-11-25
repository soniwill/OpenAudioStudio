# Design

## Context

Consulte `proposal.md` para a motivação e `specs/instruments/spec.md` para os requisitos funcionais.
Atualmente, a estrutura `Instrument` em `native/src/core/studio_model.h` já possui os campos para `event_ref`, `command_type` e `command_target`. O compilador `oae_bankc` e o codec `bank_writer`/`bank_reader` gravam e recuperam esses campos no chunk `EVTS`. Porém, em `native/src/core/event_instance.cpp`:
1. `trigger_instrument()` só trata `Single`, `Multi` e `Programmer`.
2. As varreduras de timeline em `start()` e `update()` filtram explicitamente ignorando `InstrumentKind::Event` e `InstrumentKind::Command`.

## Goals / Non-Goals

**Goals:**
- Implementar o ciclo de vida e a sub-mixagem de instâncias filhas geradas por `Event Instruments` dentro de `EventInstance`.
- Implementar o despacho de comandos (`Command Instruments`) na game thread durante `oae_system_update()` / `EventInstance::update()`.
- Garantir segurança lock-free e ausência de alocações dinâmicas na thread de render de áudio (`HeapGuard`).

**Non-Goals:**
- Implementar o `Scatterer Instrument` nesta change (reservado para a Fase 2).
- Suportar instâncias filhas recursivas infinitas (o compilador e o Studio já validam grafos acíclicos).

## Decisions

### 1. Roteamento de Áudio de Event Instruments (Hierárquico / FMOD padrão)
- **Decisão**: A instância filha gerada por um Event Instrument terá seu barramento de saída conectado diretamente ao `track_bus` da faixa do evento pai. Se a faixa não tiver bus dedicado, roteia para o bus de saída do pai.
- **Alternativas consideradas**: Roteamento direto no master bus do filho (descartado por ignorar fader, efeitos de faixa e automações da pista onde o instrumento foi colocado no Studio).

### 2. Gerenciamento de Ciclo de Vida da Instância Filha
- **Decisão**: `EventInstance` manterá um vetor de instâncias filhas ativas (`struct ActiveChildEvent { size_t track_idx; size_t inst_idx; oae_event_instance instance; }`).
  - Quando a timeline atinge o offset: cria a instância via `oae_system_create_event_instance_from_guid()` ou método interno C++, vincula parâmetros se houverem e chama `start()`.
  - Quando a timeline ultrapassa o final do instrumento ou o evento pai é interrompido: chama `stop(OAE_STOP_ALLOW_FADEOUT)` na instância filha.
  - Na destruição do pai ou descarregamento do banco: libera a instância filha via `release()`.

### 3. Execução de Command Instruments
- **Decisão**: Command Instruments não geram canais de áudio nem consomem vozes. Eles emitem ações de sistema no loop de atualização:
  - `start_event`: busca a descrição pelo `command_target` (Guid) e dispara uma nova instância independente (fire-and-forget controlada pelo sistema).
  - `stop_event`: localiza instâncias ativas do evento alvo e chama `stop()`.
  - `snapshot_on`: obtém a descrição do snapshot e chama `start()`.
  - `snapshot_off`: obtém a descrição do snapshot e chama `stop()`.

## Risks / Trade-offs

- **[Risco] Sobrecarga de vozes por eventos aninhados disparando múltiplos canais** → Mitigação: Instâncias filhas respeitam o sistema global de voz e prioridades (`oae_system_update()`).
- **[Risco] Reentrância ou travamento em `stop()` recursivo** → Mitigação: Chamadas de parada em instâncias filhas usam flags atômicas e listas pré-alocadas sem mutação concorrente de iteradores.

## Open Questions

- Nenhuma para a Fase 1. A parametrização fina de eventos aninhados poderá ser expandida na Fase 2.
