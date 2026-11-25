# Verificação de studio-stopped-playhead-seek

## Audição local no Studio Release

Foi usada uma cópia de `example_project` com o clipe em loop de `event:/music/theme` estendido para 12 segundos. A prévia indicava `Preview In Sync (Rev 0)` e o snap estava em `1 Beat (1/4)`.

| Ação | Resultado observado |
| --- | --- |
| Clicar em 2,5 segundos na régua com a audição parada e aguardar 1,2 segundo | Os contadores do transporte e da timeline continuaram em `00:02.500`. A régua e o minimapa exibiram o deslocamento do ponteiro. |
| Pressionar Play | O botão indicou reprodução e ambos os contadores avançaram de `00:02.500` para `00:02.960` na captura seguinte. |
| Acionar Stop com a posição marcada e repetir após Play | Os dois contadores retornaram a `00:00.000`, com a régua e o minimapa no início. |
| Arrastar na régua até aproximadamente 1,85 segundo | O snap encaixou o ponteiro em `00:02.000`. |
| Selecionar `event:/sfx/coin` após o arraste | A timeline exibiu o novo evento e os contadores voltaram a `00:00.000`. |

O [manual offline](../../../../docs/manual/chapters/03-transporte-e-audicao.html#estados) descreve o clique em `00:02.500`, o início da prévia com Play e a limpeza por Stop ou troca de evento.

## Gates

- `--smoke-transport-mixer` passou em Release e verificou o início da instância em 250 ms, áudio renderizado, limpeza da posição, falha de Play, pausa, retomada e seek durante reprodução.
- `./scripts/verify.ps1 -Layer studio,docs` retornou `RESULT: OK`. Foram aprovados 11/11 smokes Release, incluindo `--smoke-live`, 10/10 itens de validação estrita do OpenSpec e os links de documentação verificados pelo gate.
- O smoke `--smoke-pro-ui` falhou ao restaurar painéis na primeira execução, que reutilizou o diretório com estado de layout persistido. O mesmo binário passou em diretório limpo. Na execução completa aprovada, cada smoke usou um diretório limpo e o bundle saiu em `build/stopped-playhead-qa-a0ceb0d6/verified-bundle` para preservar a sessão de Studio que já estava aberta.
- `openspec validate studio-stopped-playhead-seek --type change --strict` aprovou a change.
- Em 2026-11-15, antes do arquivamento, `./scripts/verify.ps1 -All` retornou `RESULT: OK` fora do sandbox, com o layout de teste do Studio isolado e o Unity Editor fechado antes da execução em batch. Resultados do gate completo incluem CTest Debug e Release 2/2 cada, Studio Release 11/11, .NET 32/32, Unity EditMode 5/5 e PlayMode 28/28 no projeto padrão. A validação estrita de documentação passou com 11/11 itens.
