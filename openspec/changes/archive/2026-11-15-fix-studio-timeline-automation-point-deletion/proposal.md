# Proposal

## Why

A faixa `Volume Automation` da Timeline permite inserir e arrastar pontos, mas não excluir um ponto individual. Como a faixa não recebe foco de teclado, `Delete` e `Backspace` chegam ao `TimelinePanel` e removem o clipe selecionado, obrigando o usuário a desfazer a inserção com `Ctrl+Z`.

## What Changes

- Permitir excluir um ponto da faixa `Volume Automation` com `Delete`/`Backspace` quando a faixa tem foco, e também pelo menu de contexto do ponto. A exclusão consome a tecla e não altera clipes ou itens lógicos.
- Excluir o último ponto restante remove a curva temporal de volume da pista, devolvendo a faixa à grade vazia. As demais curvas da pista mantêm origem, alvo, ordem e pontos.
- Registrar cada exclusão como um único passo de Undo/Redo, preservando o comportamento de gesto já usado em arraste e inserção.
- Dar foco de teclado à faixa ao clicar em um ponto, de modo que o tratamento da tecla ocorra antes do `TimelinePanel`. O foco da timeline SHALL seguir o último alvo clicado, então o clipe também passa a receber foco ao ser clicado, mantendo `Delete` sobre o clipe quando ele for o último alvo.
- Atualizar o capítulo 5 do manual offline com o gesto de exclusão e o comportamento da tecla.

Fora do escopo: alterar a inserção por duplo clique, o arraste de pontos, o formato de curvas no banco, a avaliação no engine e a atribuição de foco da régua e do cabeçalho da pista.

## Capabilities

### New Capabilities

### Modified Capabilities

- `studio-shell` — acrescentar a remoção de pontos e o comportamento de foco/tecla à faixa `Volume Automation` da Timeline.

## Impact

- Studio — `AutomationLaneComponent` (foco, seleção de ponto, `keyPressed`, menu de contexto), `ClipComponent` (foco ao clique) e `AuthoringModel` (operação de limpeza da curva temporal de volume).
- Smoke integrado do Studio (`studio/Source/Main.cpp`), manual offline (capítulo 5) e validação estrita da change.
- Engine, banco e Unity — sem alteração de formato ou runtime: a ausência da curva já contribui com `0.0` em `curve_eval` e o compilador de banco aceita a remoção da entrada.
