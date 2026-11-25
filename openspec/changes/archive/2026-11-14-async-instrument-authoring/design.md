# Design

## Context

Veja `proposal.md` para motivação e escopo. O motor nativo (`EventInstance` em `native/src/core/event_instance.cpp`) e o formato `.bank` (`bank_writer.cpp` / `bank_reader.cpp`) já possuem `Instrument::async` implementado e testado em C++ (`SustainPoint` não pausa `!v.async`, `stop_sync_voices_outside` ignora `!v.async`, `AsyncOverlapPolicy` respeita `inst.async`). O único gap está na camada de autoria/publicação (`project-v1.schema.proposed.json`, `project_parser.h` e `InspectorPanel.cpp`).

## Goals / Non-Goals

**Goals:**
- Expor `async` (booleano, padrão `false`) no schema JSON de projeto e no parser do `oae_bankc`.
- Expor o toggle **Async** no `InspectorPanel` para instrumentos compatíveis (`single`, `multi`, `scatterer`, `programmer`, `event`), integrado ao fluxo de Undo/Redo do `AuthoringModel`.
- Demonstrar o comportamento completo com `event:/sfx/charge_sustain_async` em `example_project/project.json` (`SustainPoint` + `async: true` + `loop: true` + `AHDSR` com `release_on_key_off: true`).

**Non-Goals:**
- Alterar o formato binário `.bank` ou a ABI C (`c-api-abi`).
- Alterar o comportamento de `SustainPoint` para congelar/granularizar áudio síncrono.

## Decisions

1. **Schema e Parser (`oae_bankc`)**:
   - **Decisão:** Adicionar `"async": { "type": "boolean" }` nos blocos de propriedades de instrumentos em `docs/specs/project-v1.schema.proposed.json` (`single`, `multi`, `scatterer`, `programmer`, `event`) e ler `in.async = inst.value("async", false);` em `native/tools/bankc/project_parser.h`.
   - **Alternativa considerada:** Criar um novo tipo de instrumento `async_single`. Rejeitada porque no modelo FMOD Studio e no `studio_model.h` do OAE, `async` é um atributo ortogonal aplicável a múltiplos tipos de instrumento.

2. **UI no Studio (`InspectorPanel`)**:
   - **Decisão:** Exibir um `juce::ToggleButton` ("Async") na seção de propriedades do instrumento selecionado em `studio/Source/UI/InspectorPanel.cpp`, editando o campo `async` via transação do `AuthoringModel` (mesmo padrão dos toggles/controles existentes).
   - **Alternativa considerada:** Exibir apenas em `Single`. Rejeitada porque `Multi`, `Scatterer`, `Programmer` e `Event` também suportam `async` no runtime.

3. **Exemplo no `example_project`**:
   - **Decisão:** Adicionar `event:/sfx/charge_sustain_async` com `charge_up` (`0..600 ms`), `SustainPoint` (`600 ms`), `charge_hold` na Track 2 (`offset_ms: 550 ms`, `async: true`, `loop: true`, `AHDSR` com `attack_ms: 80`, `release_ms: 250`, `release_on_key_off: true`) e `charge_release` (`600..1300 ms`).
   - **Por que `offset_ms: 550 ms`:** Inicia `charge_hold` imediatamente antes de atingir o `SustainPoint` em `600 ms`, garantindo que o som sustentado já esteja ativo quando a timeline congelar em `600 ms` e faça fade-out suave no `Key Off`.

## Risks / Trade-offs

- **[Risco] Instrumento `async` sem `AHDSR` nem `loop: false` continuar tocando indefinidamente após o fim do evento** → **Mitigação:** No exemplo `charge_sustain_async`, combinar `async: true` + `loop: true` com modulador `AHDSR` (`release_on_key_off: true`), e documentar esse padrão no projeto de exemplo.
- **[Risco] `schema_validator` falhar se algum tipo de instrumento no schema tiver `additionalProperties: false` sem `async`** → **Mitigação:** Atualizar todos os blocos de instrumento compatíveis em `docs/specs/project-v1.schema.proposed.json` e validar via `oae_bankc` e `native` CTest.
