# timeline-logic Specification

## Purpose

Definir a especificação normativa de marcadores e regiões lógicas de timeline (`marker`, `tempo`, `loop_region`, `sustain_point`, `transition_marker`, `transition_region`, quantização `immediate`/`beat`/`bar` e `Parameter Conditions`) na camada de autoria e execução.

## Requirements

### Requirement: Autoria — Marcadores e regiões lógicas de timeline
O `AuthoringModel` MUST permitir criar, editar, mover, redimensionar e remover itens lógicos em eventos (`marker`, `tempo`, `loop_region`, `sustain_point`, `transition_marker` e `transition_region`) com suporte completo a Undo/Redo.

#### Scenario: Criação e edição de transition_region e loop_region
- **WHEN** o usuário adiciona uma `transition_region` com `target_marker`, `length_ms`, `quantization` e `crossfade_ms`, ou ativa `until_key_off` em uma `loop_region`
- **THEN** o `AuthoringModel` valida e persiste os campos no array `logic_items` do evento com suporte a Undo/Redo e compilação limpa via `oae_bankc`

#### Scenario: Renomeação de marker com integridade referencial
- **WHEN** um `marker` referenciado por `target_marker` em transições tem seu nome alterado
- **THEN** o `AuthoringModel` atualiza o nome do `marker` e sincroniza as referências `target_marker` correspondentes no mesmo evento em uma única transação de Undo/Redo

### Requirement: Autoria — Marcadores de tempo e condições por parâmetro
O `AuthoringModel` MUST suportar edição de marcadores `tempo` (`bpm > 0`, `ts_upper >= 1`, `ts_lower` potência de 2 válida) e condições `Parameter Conditions` (`terms` com parâmetro/intervalo `[min, max]` e `probability_pct` em `[0, 100]`) em `loop_region`, `transition_marker` e `transition_region`.

#### Scenario: Edição de Tempo Marker e condição de transição
- **WHEN** o usuário ajusta `bpm`, `ts_upper` e `ts_lower` de um marcador `tempo` e configura uma condição por parâmetro e probabilidade em uma `transition_region`
- **THEN** os valores normalizados são persistidos no projeto e preservados na compilação para `.bank`

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
