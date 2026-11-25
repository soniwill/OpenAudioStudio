# Proposal

## Why

O botão `A` do cabeçalho muda a cor e a altura da pista, mas o editor de automação continua oculto. O teste via MCP reproduziu a falha inclusive com dois pontos de automação de volume no evento `event:/music/theme`.

## What Changes

- Fazer o botão `A` mostrar e ocultar o editor de automação da pista correspondente, mantendo o indicador visual e a geometria sincronizados.
- Exibir a curva existente quando houver pontos e a grade vazia quando a pista estiver sem curva. A expansão é um estado visual da timeline.
- Cobrir o clique real no cabeçalho com uma regressão de integração e verificar o resultado no Studio Release via MCP.
- Documentar o botão no capítulo de timeline do manual offline. A change se limita à exibição e ao acesso à edição existente da curva. Criação de curvas, seleção de alvos e comportamento de audição ficam fora do escopo.

## Capabilities

### New Capabilities

### Modified Capabilities

- `studio-shell` — especificar a expansão e o recolhimento da faixa de automação pelo botão `A` de cada pista.

## Impact

- **Studio** — comunicação entre `TrackHeaderPanel`, `TimelinePanel` e `TimelineTrackLane`, com cobertura de integração e atualização do manual.
- **MCP** — reutilização de inspeção, clique e captura existentes para verificar o Studio Release.
- **Engine, bancos e Unity** — sem impacto nas APIs, nos formatos ou na semântica de reprodução.
