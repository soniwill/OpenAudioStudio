# Verificação de fix-studio-track-automation-toggle

## Studio Release via MCP

A verificação usou o bundle `build/Studio_Bundle_Release`, com executável, layout e projeto em um diretório isolado. Em `event:/music/theme`, a pista **Com curva** tinha automação de volume em 400 ms (−24 dB) e 1.400 ms (0 dB). A pista **Sem curva** não tinha automação cadastrada.

`studio_ui_inspect` localizou os botões `A` pela legenda e pelos limites em coordenadas da janela. `studio_ui_mouse` acionou os botões pelo centro. Após cada clique, a inspeção confirmou a visibilidade do editor e a altura das duas pistas.

| Captura de `timeline_view` por `studio_ui_screenshot` | Alturas em px | Observação |
| --- | --- | --- |
| 01 — Ambas recolhidas (evidência local) | 80 / 80 | Botões sem destaque e editores ocultos. |
| 02 — Curva expandida (evidência local) | 140 / 80 | Primeira faixa com curva e dois pontos, segunda recolhida. |
| 03 — Ambas expandidas (evidência local) | 140 / 140 | Primeira faixa mantém a curva, segunda mostra o rótulo `Volume Automation` e a grade vazia. |
| 04 — Curva recolhida (evidência local) | 80 / 140 | A segunda faixa permanece expandida. |
| 05 — Ambas recolhidas novamente (evidência local) | 80 / 80 | Captura idêntica à inicial por SHA-256. |
| 06 — Curva reaberta (evidência local) | 140 / 80 | Captura idêntica à primeira expansão por SHA-256. |

O editor visível mediu 518 × 60 px, com início 80 px abaixo do topo da pista. O JSON obtido pelo recurso MCP `oae://project/state` permaneceu igual após os cliques. Os arquivos `project.json` da cópia de teste e do projeto original conservaram seus SHA-256.

No [capítulo 5 do manual offline](../../../../docs/manual/chapters/05-timeline-e-waveforms.html#automacao-bezier), o link do sumário abriu `#automacao-bezier` no Chrome em modo offline. Os três passos do exemplo foram conferidos no Studio Release no cenário acima.

## Gates

| Comando | Resultado |
| --- | --- |
| `./scripts/verify.ps1 -Layer studio` | `RESULT: OK`, 11/11 smokes Release. `--smoke-timeline` cobriu os cliques pelo cabeçalho, alinhamento, geometria, isolamento entre pistas e preservação do JSON. |
| `./scripts/verify.ps1 -Layer docs` | `RESULT: OK`, 10/10 itens OpenSpec estritos e verificação de links do gate. |
| `openspec validate fix-studio-track-automation-toggle --type change --strict` | Código 0, change válida. |
| `./scripts/verify.ps1 -All` antes do arquivamento | Código 0, `RESULT: OK`. OpenSpec estrito 10/10, CTest 2/2 em Debug e Release, Studio Release 11/11, .NET 32/32, Unity EditMode 5/5 e PlayMode 28/28. |

O gate Studio foi executado fora do sandbox após o shell do Git falhar ao conferir o JUCE. O smoke `--smoke-pro-ui` falhou ao restaurar painéis com o layout persistido no diretório do executável. O gate do Studio passou com esse arquivo isolado temporariamente.

Nas primeiras execuções de `-All`, o CTest atingiu o limite de tempo ou registrou falhas de recuperação de publicação. Em seguida, o Unity atingiu o limite de 300 segundos enquanto aguardava o IL Post Processor e terminou o EditMode após esse prazo. A execução final usou um diretório exclusivo no temporário do Windows e o cache do Unity já preparado pelo EditMode. Os layouts do executável e do bundle do Studio foram restaurados com SHA-256 conferido.
