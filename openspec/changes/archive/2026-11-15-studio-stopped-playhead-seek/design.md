# Design

## Context

`TimelineRuler` já converte o gesto em tempo com snap. `TimelinePanel` encaminha esse tempo para `MainComponent`, que chama `AuditionTransport::seek`. O timer de `MainComponent` atualiza a interface com a posição do transporte a 30 Hz. O transporte guarda a posição em pausa, porém retorna zero quando está parado. O setter de posição do painel atualiza a régua e o minimapa, e o contador do painel é atualizado pelo callback do gesto.

## Goals / Non-Goals

- Usar o transporte como fonte da posição da audição local e manter os indicadores coerentes entre o gesto e as atualizações do timer.
- Aplicar a posição à instância local antes de iniciar a reprodução, pelo mecanismo de posicionamento inicial que a API atual já oferece. O fluxo de gestos da régua e os comandos do Live-Linking seguem as fronteiras do [escopo](proposal.md).

## Decisions

1. **Guardar a posição no transporte enquanto parado.** `seek` registra a posição para o evento selecionado e `get_current_position_ms` a devolve nesse estado. Guardar a posição no painel exigiria que o transporte consultasse um estado de UI para iniciar a audição e que o timer conciliasse as duas fontes.
2. **Configurar a instância local antes de iniciar.** Reusar a chamada de posicionamento já usada por `resume`. O motor preserva a posição indicada antes de `start` e a considera no agendamento inicial. Um seek posterior ao start permitiria que a instância agendasse conteúdo do início. Verificar o resultado da chamada de posicionamento e da partida. Liberar uma instância parcialmente configurada em caso de falha e conservar a posição escolhida para nova tentativa.
3. **Sincronizar os indicadores pelo mesmo setter do painel.** Usar `TimelinePanel::set_playhead_time_ms` para refletir mudanças do gesto e do timer no contador do painel, na régua e no minimapa. O contador da barra de transporte acompanha a mesma leitura do transporte. A criação de instância fica no fluxo de Play.
4. **Limitar a posição ao evento.** Limpar a posição ao processar Stop, inclusive se o transporte já estiver parado, e em `set_event_path` quando o caminho mudar. A troca de caminho enquanto parado precisa de limpeza explícita, pois atualmente esse ramo dispensa `stop`.

## Risks / Trade-offs

- A regressão aparece na atualização seguinte ao gesto. Verificar a posição depois de vários ciclos do timer e conferir a partida por um evento longo com conteúdo audível no ponto escolhido.
- A troca de evento usa o caminho como identidade. Cobrir troca com o transporte parado, que pode deixar uma posição antiga mesmo sem instância ativa.
- Dois fluxos de Play compartilham o transporte. Verificar que a posição escolhida para a audição local não altera o comando remoto existente.
