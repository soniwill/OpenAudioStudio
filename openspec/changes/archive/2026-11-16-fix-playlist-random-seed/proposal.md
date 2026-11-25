# Proposal

## Why

O `PlaylistState` global nasce com semente fixa (`Pcg32(0x12345678, 0x9abcdef0)` em `studio_model.h`) e cada `Play` cria uma nova `EventDescription` (`event_api.cpp`), de modo que a primeira escolha de cada play é sempre idêntica. Isso viola o requisito existente "Runtime — Seleção de itens em Multi Instruments", que exige convergência estrita à distribuição de pesos: 14 plays consecutivos de uma playlist 80/20 sortearam sempre a mesma entrada (evidências em `../studio-help-workflow-accuracy/evidence/FINDINGS-1.4.md`). O exercício do manual "pré-visualize mais de uma vez para ouvir a diferença" não é verificável neste build.

## What Changes

- Semear o gerador do `PlaylistState` de forma não determinística entre plays (ex.: entropia do relógio/contador global no ponto de criação), preservando o determinismo dentro de uma mesma instância quando exigido por modos como `RandomNoRepeat`.
- Adicionar cenário ao spec canônico exigindo variação do sorteio entre plays distintos de um Multi `Random (weighted)`.
- Adicionar teste nativo de estatística: N plays de uma playlist 80/20 devem selecionar ambas as entradas, com convergência aproximada aos pesos.
- Desbloqueia a tarefa 1.4 de `studio-help-workflow-accuracy` (exercício de Multi com variação audível).

## Capabilities

### New Capabilities

(nenhuma)

### Modified Capabilities

- `instruments`: o requisito "Runtime — Seleção de itens em Multi Instruments" ganha cenário cobrindo plays independentes de um mesmo evento: sorteios de plays distintos MUST variar e convergir à distribuição de pesos, sem semente fixa por play.

## Impact

- **Motor (C/C++)**: `native/src/core/studio_model.h` (semeadura do `PlaylistState`), possivelmente `native/src/api/event_api.cpp` (ponto de criação); `native/tests/` (teste estatístico de sorteio entre plays).
- **ABI/formato**: sem mudança; o modo de playlist e o escopo (Global/Instance) permanecem como compilados.
- **Studio/Unity**: sem mudança de código; a variação audível passa a conformar o spec na prévia.
- **Determinismo de testes**: gates existentes que dependem de sorteio devem continuar estáveis — teste estatístico com margem, não igualdade exata.
- **Documentação**: nenhum arquivo nesta change; a revisão do manual fica na tarefa 1.4 de `studio-help-workflow-accuracy`.
