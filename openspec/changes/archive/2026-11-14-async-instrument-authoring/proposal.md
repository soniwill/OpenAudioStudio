# Proposal

## Why

O motor de áudio nativo (`EventInstance`) e o formato binário `.bank` já implementam o comportamento completo de instrumentos assíncronos (`Instrument::async`), incluindo continuidade durante `SustainPoint` e `LoopRegion` além do fim da região na timeline. No entanto, o campo `async` ainda não está exposto no schema/parser de `project.json` (`oae_bankc`) nem na interface de autoria do OpenAudio Studio (`InspectorPanel`), impedindo que projetos de áudio usem instrumentos assíncronos para sustentar notas/texturas contínuas enquanto aguardam `Key Off`.

## What Changes

- **Schema e compilador (`oae_bankc`):** Expor a propriedade booleana opcional `async` (padrão `false`) nos tipos de instrumento compatíveis em `docs/specs/project-v1.schema.proposed.json` e lê-la em `native/tools/bankc/project_parser.h` (`in.async = inst.value("async", false)`).
- **OpenAudio Studio (`InspectorPanel` / `AuthoringModel`):** Adicionar o controle **Async** (`ToggleButton`) no `InspectorPanel` para instrumentos selecionados (com suporte a Undo/Redo via `AuthoringModel`), e refletir visualmente o estado assíncrono quando aplicável.
- **Exemplo funcional (`example_project`):** Adicionar `event:/sfx/charge_sustain_async` em `example_project/project.json` combinando `SustainPoint` (`600 ms`), `charge_up` (`0..600 ms`), `charge_hold` (`async: true`, `loop: true`, `AHDSR` com `release_on_key_off: true`) e `charge_release` (`600..1300 ms`), recompilar `example_project/build/master.bank` e sincronizar o bundle.
- **Testes automatizados:** Cobrir parsing/compilação de `async` em `oae_bankc`/testes nativos e a edição/persistência de `async` no smoke test do Studio.
- **Exclusões:** Não altera o formato binário `.bank` (o campo `async` já existe em `bank_writer.cpp`/`bank_reader.cpp`), não altera a ABI C (`c-api-abi`) e não introduz novos tipos de instrumento.

## Capabilities

### New Capabilities
<!-- Nenhuma nova capacidade de nível de sistema; requisitos adicionados às capacidades existentes abaixo. -->

### Modified Capabilities
- `instruments`: Expor a propriedade `async` no schema/compilador `project.json` (`oae_bankc`) e no `AuthoringModel`, preservando compatibilidade com projetos existentes sem o campo (`async = false`).
- `studio-shell`: Exibir e editar a propriedade `async` de instrumentos selecionados no `InspectorPanel` com suporte a Undo/Redo.

## Impact

- **Native / Bank Compiler:** `docs/specs/project-v1.schema.proposed.json`, `native/tools/bankc/project_parser.h`, testes em `native/tests/`.
- **Studio (JUCE):** `studio/Source/UI/InspectorPanel.cpp`, `studio/Source/Model/AuthoringModel.h/.cpp` (se houver helper dedicado), smoke tests em `studio/Source/Main.cpp`.
- **Example Project:** `example_project/project.json`, `example_project/build/master.bank`.
