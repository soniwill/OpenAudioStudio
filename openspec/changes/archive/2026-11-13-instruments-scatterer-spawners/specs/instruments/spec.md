## ADDED Requirements

### Requirement: Runtime — Agendamento estocástico e polifonia de Scatterer Instruments
Enquanto um `Scatterer Instrument` estiver ativo na timeline ou sob condição válida, o motor de áudio MUST disparar grãos da sua playlist em intervalos sorteados deterministicamente (via `PCG32`) entre `spawn_interval_min_ms` e `spawn_interval_max_ms`, respeitando o limite simultâneo `polyphony` e o teto opcional `total_sounds` (`0` = contínuo enquanto ativo).

#### Scenario: Disparo periódico respeitando intervalo e polifonia
- **WHEN** um evento contendo um `Scatterer Instrument` com `polyphony = 2` e `spawn_interval_min_ms = 50`, `spawn_interval_max_ms = 100` permanece ativo em `EventInstance::update()`
- **THEN** o motor dispara novos grãos respeitando a janela de intervalo configurada
- **AND** nunca mantém mais de 2 grãos ativos simultaneamente para aquele spawner

#### Scenario: Encerramento do Scatterer ao sair da região ou parar evento
- **WHEN** o cursor de timeline sai da região de um `Scatterer Instrument` síncrono ou o evento pai recebe `stop()`
- **THEN** o spawner cessa novos disparos imediatamente e propaga a parada aos grãos ativos conforme o modo de parada

### Requirement: Runtime — Dispersão espacial e variação por grão em Scatterer Instruments
Para cada grão disparado por um `Scatterer Instrument`, o motor de áudio MUST aplicar as variações configuradas de volume/pitch e calcular uma distância aleatória em `[min_scatter_distance, max_scatter_distance]` e ângulo azimutal via `PCG32`, aplicando panning estéreo e atenuação proporcional como fallback 2D quando o evento não possuir posicionamento 3D completo.

#### Scenario: Grãos recebem dispersão estéreo e atenuação determinística
- **WHEN** um `Scatterer Instrument` dispara múltiplos grãos com `max_scatter_distance > min_scatter_distance`
- **THEN** cada grão recebe ganho atenuado proporcionalmente à distância sorteada e deslocamento panorâmico distribuído entre esquerda e direita
