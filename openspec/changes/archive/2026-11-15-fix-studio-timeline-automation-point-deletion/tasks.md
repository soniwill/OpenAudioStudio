# Tasks

## 1. Modelo e interação da faixa

- [x] 1.1 Em `AuthoringModel`, adicionar `clear_track_timeline_volume_points(event_path, track_index)` que remove somente a entrada `target=volume`, `is_timeline=true` via `modify`, preservando origem, alvo, ordem e pontos das demais curvas. Verificar que a operação integra o snapshot de Undo/Redo e que o smoke da tarefa 2.2 restaura a curva com o ponto original após um único `Ctrl+Z`.
- [x] 1.2 Em `AutomationLaneComponent`, adicionar `setWantsKeyboardFocus(true)`, captura de foco no `mouseDown`, `selected_point_index_` com clamp, limpeza da seleção em `focusLost`, `keyPressed` para `Delete`/`Backspace` consumindo a tecla e menu de contexto com exclusão dentro de `begin_gesture`/`end_gesture`. Validar o índice relendo os pontos antes de excluir e remover a curva quando restar um único ponto. Verificar pelo smoke da tarefa 2.1 e pela checagem de foco da tarefa 2.3.
- [x] 1.3 Em `ClipComponent`, pedir foco de teclado e capturá-lo no `mouseDown` sem tratar teclas, para que `Delete` atue sobre o clipe quando ele for o último alvo clicado. Verificar pelo smoke da tarefa 2.3.

## 2. Smoke integrado e manual

- [x] 2.1 No smoke da Timeline em `studio/Source/Main.cpp`, cobrir a exclusão por `Delete` com outras curvas na pista: conferir remoção apenas do ponto selecionado, preservação das demais curvas, um passo de Undo/Redo, ausência de efeito sem ponto selecionado e o mesmo resultado pelo caminho do menu de contexto. Verificar com a execução do smoke do Studio retornando sucesso.
- [x] 2.2 No mesmo smoke, cobrir a exclusão do último ponto: conferir remoção da entrada temporal de volume, grade vazia, outras automações inalteradas e restauração completa por `Ctrl+Z` com um único passo. Verificar com a execução do smoke retornando sucesso.
- [x] 2.3 No mesmo smoke, cobrir o roteamento da tecla: exclusão de ponto não remove clipe selecionado e, após clicar no clipe, `Delete` remove o clipe sem alterar pontos e curvas. Verificar com a execução do smoke retornando sucesso.
- [x] 2.4 Atualizar o capítulo 5 do manual offline com o gesto de exclusão (tecla e menu de contexto), o comportamento do último ponto e o Undo/Redo, sem usar `Delete` para editar ponto fora da faixa focada. Verificar a navegação e os rótulos no manual offline.

## 3. Verificação integrada

- [x] 3.1 Executar `./scripts/verify.ps1 -Layer studio`, `./scripts/verify.ps1 -Layer docs` e `openspec validate fix-studio-timeline-automation-point-deletion --type change --strict`. Com o Studio em execução e prévia ativa, usar as ferramentas MCP `studio_ui_mouse` e `studio_ui_key` para excluir um ponto, excluir o último ponto e confirmar por `studio_ui_inspect` que a curva sumiu e o áudio segue sem erro. Registrar as evidências na change.
