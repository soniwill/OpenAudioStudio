# Verificação de harness-foundation

## Resultado de 2026-11-13

- `openspec validate harness-foundation --strict` e `./scripts/verify.ps1 -Layer docs` passaram. A checagem documental cobre os links dos pontos de entrada, o índice de 19 capacidades e 37 documentos e o limite de 100 linhas da memória.
- Os dois blueprints foram reconhecidos como renomeações R100. A cópia da memória e os dois relatórios movidos mantiveram seus hashes SHA-256. Os três arquivos efêmeros especificados estavam ausentes do disco e de `git status --short` na conferência.
- `git submodule status -- studio/juce` confirmou `29396c22c93392d6738e021b83196283d6e4d850` (`8.0.12`). O índice registra modo `160000`. A detecção incremental retornou `docs, studio`.
- `./scripts/verify.ps1 -All` no sandbox atingiu o timeout de 120 segundos no teste CTest `native` em Debug. Fora do sandbox, passaram 2/2 testes CTest em cada configuração, Debug e Release. Na primeira execução externa, o smoke Live-Linking falhou ao abrir a porta 7450. Passou na repetição isolada e no gate completo seguinte, em que o Studio Release passou 11/11 smokes.
- Antes da correção, Unity PlayMode passou 22/27. Os cinco testes de `V1Audio2DTests` falharam no `SetUp` porque a busca partia de `AppDomain.CurrentDomain.BaseDirectory` e terminava na pasta de instalação do Editor.
- A suíte V1 passou a ler os bancos de exemplo de `Application.streamingAssetsPath` no Unity. Em .NET standalone, a busca anterior permanece. Os bancos distribuídos em `Assets/StreamingAssets` e em `example_project` têm hashes correspondentes.
- Após a correção, a primeira execução do gate completo falhou no teste T13 em Release com acesso negado durante uma substituição atômica. A repetição fora do sandbox com `TEMP` e `TMP` apontando para uma pasta temporária isolada sob `build/` retornou `RESULT: OK` (exit 0). CTest passou 2/2 em cada configuração, Studio Release 11/11, .NET standalone 31/31, Unity EditMode 5/5 e PlayMode 27/27, incluindo os cinco testes V1. Os relatórios do Unity estão em `build/UnityTestResults/`.

O OpenSpec registra 9/9 tarefas concluídas e o gate completo foi aprovado.
