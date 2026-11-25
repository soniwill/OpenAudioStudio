# Spec Delta

## MODIFIED Requirements

### Requirement: Runtime — Seleção de itens em Multi Instruments
O motor de áudio MUST sortear ou iterar entradas de playlist de um Multi Instrument em conformidade com o modo de playlist configurado (Random, RandomNoRepeat, Sequential, Shuffle) e escopo de seleção (Global ou Instance). Em modos aleatórios, sorteios de plays distintos de um mesmo evento MUST ser independentes, sem semente fixa por play, e MUST convergir à distribuição de pesos configurada.

#### Scenario: Seleção determinística por peso
- **WHEN** um Multi Instrument for disparado
- **THEN** a entrada da playlist selecionada MUST convergir estritamente para a distribuição de pesos configurada utilizando o gerador pseudoaleatório PCG32

#### Scenario: Variação de atraso estocástico
- **WHEN** um Multi Instrument possuir `delay_ms_random_range` maior que zero
- **THEN** o disparo da voz MUST ser atrasado deterministicamente por um valor aleatório entre zero e o limite máximo configurado

#### Scenario: Sorteios independentes entre plays distintos
- **WHEN** o mesmo evento com Multi Instrument `Random (weighted)` é reproduzido múltiplas vezes a partir de plays independentes
- **THEN** a primeira escolha de cada play MUST variar entre plays, sem repetição idêntica sistemática imposta por semente fixa
- **AND** o conjunto de escolhas observadas MUST conter mais de uma entrada da playlist quando ambas têm peso não nulo
