# Tasks

## 1. Audição local e indicadores do Studio

- [x] 1.1 Em `AuditionTransport`, guardar e ler a posição escolhida com o evento parado, aplicá-la antes de iniciar a instância local, tratar falhas de partida e limpar a posição em Stop e na troca de evento. Ampliar `--smoke-transport-mixer` para verificar seek parado sem instância ativa, posição inicial em Play, limpeza por Stop e troca de evento, além dos caminhos de pausa e seek em reprodução.
- [x] 1.2 Em `TimelinePanel` e na integração com `MainComponent`, aplicar a posição aos indicadores pelo caminho compartilhado do gesto e do timer. Exercitar com uma prévia local pronta e um evento com mais de três segundos. Clicar na régua em `00:02.500`, conferir ponteiro, contadores e minimapa após pelo menos um segundo e pressionar Play para verificar que a prévia parte dali. Exercitar arraste com snap, Stop e troca de evento.
- [x] 1.3 Atualizar `docs/manual/chapters/03-transporte-e-audicao.html` com o procedimento local testado e a limpeza por Stop e troca de evento. Conferir a seção no manual offline e registrar o evento e os passos usados.

## 2. Verificação integrada

- [x] 2.1 Executar `./scripts/verify.ps1 -Layer studio,docs` e `openspec validate studio-stopped-playhead-seek --type change --strict`. Registrar as evidências do fluxo na interface e os resultados dos gates, incluindo o smoke de Live-Linking que exercita o transporte remoto.
