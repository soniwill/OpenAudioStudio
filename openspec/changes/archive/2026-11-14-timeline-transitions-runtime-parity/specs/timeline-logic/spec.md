# Spec Delta

## ADDED Requirements

### Requirement: Runtime — Resolução de marcadores de destino e execução de TransitionMarker e TransitionRegion
O motor de áudio MUST resolver destinos de transição procurando marcadores `Marker` pelo nome (mantendo compatibilidade com `TransitionMarker` sem destino), limitar o pré-agendamento inicial à frente do cursor e avaliar `TransitionMarker` pontual e `TransitionRegion` contínuo com condição de parâmetro e quantização `Immediate`, `Beat` ou `Bar`.

#### Scenario: Salto imediato e condicional em TransitionMarker para Marker canônico
- **WHEN** um evento compilado contém um marcador `Marker` de destino e o cursor cruza um `TransitionMarker` cuja condição de parâmetro é verdadeira e `quantization` é `Immediate`
- **THEN** o cursor da timeline salta imediatamente para a posição do marcador `Marker` de destino

#### Scenario: Salto quantizado por Beat ou Bar em TransitionMarker e TransitionRegion
- **WHEN** o cursor ativa um `TransitionMarker` ou `TransitionRegion` com quantização `Beat` ou `Bar`
- **THEN** o salto para o marcador `Marker` de destino é adiado até o cruzamento exato da próxima fronteira de batida ou compasso

#### Scenario: Ignorar transição quando condição de parâmetro é falsa
- **WHEN** o cursor cruza um `TransitionMarker` cuja condição de parâmetro avalia como falsa
- **THEN** o cursor prossegue linearmente pela timeline sem executar o salto

### Requirement: Runtime — Crossfade de áudio em saltos de transição na timeline
Quando um `TransitionMarker` ou `TransitionRegion` executa salto com `crossfade_ms > 0`, o motor de áudio MUST aplicar fade-out gradual nas vozes síncronas anteriores e fade-in gradual de mesma duração nas vozes síncronas disparadas na posição de destino.

#### Scenario: Sobreposição suave com crossfade_ms positivo durante salto
- **WHEN** uma transição com `crossfade_ms > 0` salta para o marcador de destino enquanto uma voz síncrona está tocando
- **THEN** a voz anterior permanece tocando em fade-out durante a janela de crossfade até encerrar e a nova voz síncrona inicia seu fade-in até ganho unitário
