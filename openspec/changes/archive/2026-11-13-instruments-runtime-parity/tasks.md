# Tasks

## 1. Suporte a Command Instruments no Motor (`native`)

- [x] 1.1 Implementar a estrutura e o despacho de `CommandType::StartEvent`, `StopEvent`, `SnapshotOn` e `SnapshotOff` em `EventInstance::update()` e `EventInstance::start()`, verificando com teste unitário que comandos de disparo e parada acionam os eventos correspondentes.
- [x] 1.2 Adicionar teste CTest em `native/tests/studio_events.cpp` exercitando a ativação de snapshot e parada de evento via Command Instrument, validando com `./scripts/verify.ps1 -Layer native`.

## 2. Suporte a Event Instruments (Instâncias Aninhadas) no Motor (`native`)

- [x] 2.1 Adicionar rastreamento de instâncias filhas ativas em `EventInstance` (`active_child_events_`), conectando o barramento de saída do evento filho ao barramento da pista do pai e propagando comandos de parada (`stop()`) e liberação (`release()`).
- [x] 2.2 Integrar o gatilho de Event Instruments nas varreduras de timeline em `EventInstance::start()` e `EventInstance::process_timeline_and_logic()`, verificando com teste unitário que a timeline do evento pai instancia o filho e renderiza seu áudio composto.
- [x] 2.3 Adicionar caso de teste canônico em `native/tests/studio_events.cpp` validando que um evento com Event Instrument toca o áudio do filho na saída da pista do pai e respeita fadeout ao parar, validando com `./scripts/verify.ps1 -Layer native`.

## 3. Rastreabilidade e Governança Documental (`docs`)

- [x] 3.1 Atualizar `docs/specs/README.md` marcando a capacidade `instruments` como migrada para o spec canônico `openspec/specs/instruments/spec.md`.
- [x] 3.2 Atualizar `docs/ai/memory.md` registrando a conclusão das pendências de execução de `EVENT` e `COMMAND` no runtime, verificando com `./scripts/verify.ps1 -Layer docs`.

## Workflow follow-up

- Executar `./scripts/verify.ps1 -All` antes de arquivar a change (`openspec archive "instruments-runtime-parity"`).
