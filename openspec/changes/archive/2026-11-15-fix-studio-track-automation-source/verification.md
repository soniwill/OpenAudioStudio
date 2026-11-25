# Verificação de fix-studio-track-automation-source

## Smoke da Timeline

O `--smoke-timeline` do Studio Release comparou os pixels do editor com a curva temporal sozinha e com a curva por parâmetro antes e depois dela no array da pista. Nas duas ordens, comparou o JSON completo das automações após arrastar, inserir, desfazer e refazer. Em uma pista com curva por parâmetro e curva temporal de pitch, verificou a grade vazia, a criação da curva temporal de volume por duplo clique, a preservação da ordem das entradas e o Undo/Redo.

## Studio Release via MCP

Usei uma cópia isolada de `example_project` e do bundle Release. Em `event:/music/theme`, a primeira pista começou só com `MasterVolume → volume`, com pontos em `0` (−24 dB, `ease_out`) e `1` (0 dB, `linear`). Localizei o botão `A`, a faixa e o rótulo do Inspector com `studio_ui_inspect`. Acionei os gestos com `studio_ui_mouse` e capturei os resultados com `studio_ui_screenshot`.

| Captura | Resultado |
| --- | --- |
| 01 — Grade vazia (evidência local) | `A` expandiu a faixa para 518 × 60 px. O projeto inteiro permaneceu idêntico ao estado inicial. |
| 02 — Primeiro ponto temporal (evidência local) | O duplo clique acrescentou a curva temporal de volume com um ponto em 600 ms e −35 dB. O Inspector manteve a curva por parâmetro. |
| 03 — Ponto temporal arrastado (evidência local) | O arraste levou o ponto a 1.150 ms e cerca de −17 dB. |
| 04 — Curva por parâmetro preservada (evidência local) | O cartão `TRACK PARAM CURVES` continuou exibindo `MasterVolume → volume`, com os dois pontos e a forma `ease_out`. |

O recurso MCP `oae://project/state` confirmou que, após os gestos, a única mudança em relação ao estado inicial foi a nova curva temporal. O arquivo `project.json` de origem permaneceu idêntico byte a byte.

## Manual offline

O Chrome headless abriu a seção [Automação de volume no tempo](../../../../docs/manual/chapters/05-timeline-e-waveforms.html#automacao-volume-tempo) pelo sumário do capítulo 5. O link cruzado abriu [Curvas por Parâmetro no Inspetor](../../../../docs/manual/chapters/06-inspetor-e-modulacao.html#curvas-automacao-parametro) e o link de retorno levou à seção inicial. O teste conferiu os rótulos, os passos de edição e o exemplo `[G] MasterVolume → volume`, sem exceções JavaScript.

## Gates

| Comando | Resultado |
| --- | --- |
| `./scripts/verify.ps1 -Layer studio` | `RESULT: OK`. Studio Release, 11/11 smokes, incluindo Timeline, MCP e Live-Linking. |
| `./scripts/verify.ps1 -Layer docs` | `RESULT: OK`. 10/10 itens OpenSpec estritos e links verificados pelo gate. |
| `openspec validate fix-studio-track-automation-source --type change --strict` | Código 0, change válida. |
| `./scripts/verify.ps1 -All` | `RESULT: OK`. OpenSpec estrito 10/10, CTest 2/2 em Debug e Release, Studio Release 11/11, .NET 32/32, Unity EditMode 5/5 e PlayMode 28/28. |

A verificação do Studio e a sessão MCP usaram loopback fora do sandbox. O layout persistido no diretório do executável usado pelos smokes foi isolado para o teste e restaurado com SHA-256 conferido.

No gate completo, o CTest Debug atingiu o timeout de 120 s no sandbox. Fora do sandbox, o smoke `--smoke-pro-ui` falhou na restauração dos painéis com o layout persistido. O gate completo passou fora do sandbox após isolar `oae_studio_layout.json` e `oae_studio_layout.json.legacy.bak`. Os dois arquivos foram restaurados e conferidos byte a byte.
