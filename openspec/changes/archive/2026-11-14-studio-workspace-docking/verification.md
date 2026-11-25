# Verificação de studio-workspace-docking

Verificação em Windows com build Release do Studio, em 2026-11-14.

## Gates

- `pwsh -NoProfile -File scripts/verify.ps1 -Layer studio` — 11/11 smokes passaram. O `--smoke-pro-ui` exercita geometria aninhada, prévia, Esc durante o arraste, limites dos divisores, liberação de espaço, identificação única, ordem das abas, retorno ao dock e persistência da árvore.
- `pwsh -NoProfile -File scripts/verify.ps1 -Layer docs` — validação estrita de 9 itens, links e limite da memória passaram.
- `openspec validate studio-workspace-docking --strict` — change válida.

## Aceite no Studio

A instância isolada usou o MCP local para arrastar a aba Inspector até a borda direita externa e encaixar `Diagnostics & Log` abaixo do Bottom Deck. Na área de 1280 × 800, a coluna do Inspector passou a preencher toda a região abaixo da barra de workspace e o Bottom Deck e os logs ocuparam os mesmos 957 px de largura. O ajuste do divisor alterou a altura dos logs e do Bottom Deck. Alternar para Effects Rack conservou a faixa de logs.

O preset foi salvo como `Custom 2` pelo botão `+ Save`. Após trocar para `Stage Focus`, fechar, reabrir e reaplicar `Custom 2`, os limites de Inspector, Bottom Deck e Logs coincidiram com os limites salvos. Em 1104 × 721, a disposição relativa foi preservada. Ocultar e restaurar os logs, usar `Hide All/Restore` e destacar e devolver o Inspector também conservaram as posições esperadas. Foram verificados o descarte fora de um alvo, o agrupamento e a reordenação dentro do mesmo grupo.

Um arquivo v2 real gerado pelo binário anterior conservou o preset `Custom 1` e a largura de 180 px do Inspector. A primeira gravação em v3 preservou uma cópia idêntica do arquivo v2 em `oae_studio_layout.json.legacy.bak`. Um arquivo com árvore corrente inválida recuperou `Default (Authoring)` e manteve o preset válido disponível para seleção.

## Capturas

As imagens foram geradas com `studio_ui_screenshot` no diretório local de visualizações do chat, na pasta `studio-workspace-docking`.

| Arquivo | Evidência |
| --- | --- |
| `workspace-before-restart.png` | Inspector lateral, Effects Rack ativo e faixa de logs depois de redimensionar |
| `workspace-after-restart.png` | A mesma geometria após reaplicar o preset no Studio reaberto |
| `workspace-redimensionado.png` | Posições relativas em outra dimensão de janela |
| `cabecalho-browser-minimo.png` | Título grafite em largura de 160 px |
| `cabecalho-inspector-minimo-foco.png` | Foco de teclado perceptível em largura de 180 px |
| `abas-grafite-agrupadas.png` | Abas ativa e inativa depois da reordenação |

## Observação

Com `example_project/project.json` aberto, o fechamento do Studio retornou `0xc0000005` tanto no binário anterior quanto no novo. O teste de reinício do workspace foi concluído em uma instância sem projeto aberto.
