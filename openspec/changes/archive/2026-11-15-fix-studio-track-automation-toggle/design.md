# Design

## Context

O cabeçalho guarda o estado de expansão por pista. Seu callback atual aciona o recálculo da timeline, que usa esse estado para escolher a altura da pista. A visibilidade do editor é controlada por `TimelineTrackLane::set_automation_expanded`. A pista começa com o editor oculto e o layout normal só atualiza seus limites. O smoke de curvas chama o setter diretamente.

## Goals / Non-Goals

**Objetivo.** Sincronizar a visibilidade do editor no mesmo ponto em que o layout aplica a altura de cada pista.

**Limites.** Manter o estado de expansão na UI e reaproveitar a renderização, as conversões de coordenadas e a edição por gesto existentes.

## Decisions

- Expor em `TrackHeaderPanel` uma consulta do estado de expansão por índice. Em `TimelinePanel`, aplicar o estado à `TimelineTrackLane` junto com a geometria e evitar chamar o setter quando o estado já estiver correto. Esse ponto de sincronização também cobre o layout após reconstruir as pistas e mantém uma única fonte do estado visual.
- Reusar `AutomationLaneComponent` para desenhar a curva cadastrada ou a grade vazia. A UI já fornece esse desenho, e o mesmo componente recebe a edição dos pontos.
- Exercitar o botão no painel integrado. Usar a visibilidade e a área do editor como oráculos de teste, além da restauração do layout e da integridade dos pontos. A verificação via MCP usa coordenadas obtidas da inspeção porque o botão `A` não tem `ComponentID`.

## Risks / Trade-offs

- Um teste baseado apenas na altura ou na cor do botão já passaria com a falha atual. Conferir a área visível do editor e as capturas do Studio Release com e sem curva.
- Cabeçalhos e pistas são relacionados por índice durante o layout. Usar um evento com duas pistas para confirmar que expandir uma delas preserva o estado da outra.
