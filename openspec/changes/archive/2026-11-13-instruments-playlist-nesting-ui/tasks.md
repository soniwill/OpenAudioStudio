# Tasks

## 1. Camada Banco/Compilador (`native/tools/bankc`, `native/src/bank`)

- [x] 1.1 Atualizar `native/tools/bankc/project_parser.h` para reconhecer `kind == "programmer"`, ler `programmer_key` e validar/parsear `audio_table` (chave não vazia, sem duplicatas, `sample_id` existente) — verificar com compilação de `oae_bankc` e teste unitário de rejeição/aceite em `native/tests/bank_compiler.cpp`.
- [x] 1.2 Estender a serialização e carregamento `.bank` em `native/src/bank/` para persistir `programmer_key` e `AudioTableEntry` mantendo compatibilidade com bancos v1 sem `audio_table` — verificar com roundtrip completo e execução de evento `programmer` em `native/tests/bank_compiler.cpp` via CTest (`ctest --test-dir build --output-on-failure`).

## 2. Camada Modelo de Autoria (`studio/Source/Model`)

- [x] 2.1 Adicionar suporte a `kind == "programmer"`, `programmer_key` e gerenciamento de `audio_table` (`key` -> `sample_id`) no `AuthoringModel` com Undo/Redo e propagação em renomeio/exclusão de samples — verificar via smoke/teste automatizado em `studio/Source/Main.cpp`.

## 3. Camada Studio UI (`studio/Source/UI`)

- [x] 3.1 Atualizar `PlaylistDeck` (`studio/Source/UI/PlaylistDeck.h/.cpp`) para oferecer adição de `target_event` (filtrada por `AuthoringModel::get_instrument_targets` contra ciclos) além de samples no botão `+`, exibindo entradas de evento com prefixo `[EVT]` — verificar via smoke em `studio/Source/Main.cpp`.
- [x] 3.2 Atualizar `InspectorPanel` e `TimelineTrackLane` para criar `Programmer Instrument` em `+ Add Instrument`, editar `programmer_key` e entradas da `audio_table` no `InspectorPanel` (com Undo/Redo) e renderizar o clipe `[PRG]` na timeline — verificar via smokes Release do Studio.

## 4. Documentação e Gate de Verificação

- [x] 4.1 Atualizar documentação/exemplos de autoria de playlist mista (`target_event`) e `Programmer Instrument` (`audio_table`) conforme padrão do projeto e verificar consistência com os fluxos implementados.
- [x] 4.2 Executar os gates aplicáveis (`scripts/verify.ps1` ou CTest + smokes do Studio Release) após a última edição e registrar os resultados em `docs/ai/memory.md`.
