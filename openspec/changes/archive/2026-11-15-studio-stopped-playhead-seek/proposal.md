# Proposal

## Why

Com a audição local parada, o clique na régua move o ponteiro por um instante. O transporte descarta essa posição e o próximo ciclo de atualização devolve o ponteiro ao início, impedindo que o usuário escolha de onde ouvir o evento.

## What Changes

- Guardar a posição escolhida por clique ou arraste na régua enquanto a audição local está parada e exibi-la no ponteiro e no contador.
- Iniciar a prévia local a partir da posição escolhida ao pressionar Play.
- Limpar a posição ao pressionar Stop ou trocar o evento de audição.

## Capabilities

### New Capabilities

### Modified Capabilities

- `studio-shell` — posicionamento do ponteiro e início da audição local a partir da régua com o transporte parado.

## Impact

- **Studio** — transporte local, indicadores de posição e verificação do fluxo pela régua.
- **Documentação** — capítulo Transporte e Audição do manual offline, com exemplo de início da prévia no ponto escolhido.
- **Exclusões** — cliques nas pistas, edição de itens lógicos, Parameter Sheets, busca remota via Live-Linking e alterações no motor, na API C, no formato de banco ou na Unity.
