# Tasks

## 1. Schema e Compilador (`oae_bankc`)

- [x] 1.1 Adicionar a propriedade booleana opcional `"async"` nos tipos de instrumento compatíveis em `docs/specs/project-v1.schema.proposed.json` e ler `in.async = inst.value("async", false);` em `native/tools/bankc/project_parser.h`. Verificar com compilação de `oae_bankc` e teste nativo cobrindo `async: true` e omissão de `async` em `scripts/verify.ps1 -Layer native`.

## 2. Interface de Autoria no OpenAudio Studio

- [x] 2.1 Adicionar o toggle **Async** no `InspectorPanel` (`studio/Source/UI/InspectorPanel.cpp`) para instrumentos compatíveis selecionado(s), persistindo em `AuthoringModel` com suporte a Undo/Redo, e verificar no smoke test do Studio (`scripts/verify.ps1 -Layer studio`).

## 3. Projeto de Exemplo e Validação Integrada

- [x] 3.1 Adicionar `event:/sfx/charge_sustain_async` em `example_project/project.json` (com `SustainPoint` em `600 ms`, `charge_up`, `charge_hold` `async: true` + `loop: true` + `AHDSR` `release_on_key_off: true`, e `charge_release`), compilar `example_project/build/master.bank` com `oae_bankc` e validar o bundle e gates finais (`scripts/verify.ps1 -Layer native`, `scripts/verify.ps1 -Layer studio`, `scripts/verify.ps1 -Layer docs`).
