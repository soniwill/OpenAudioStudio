# Spec Delta

## Purpose

Define a especificação normativa de marcadores e regiões lógicas de timeline (`marker`, `tempo`, `loop_region`, `sustain_point`, `transition_marker`, `transition_region`, quantização `immediate`/`beat`/`bar` e `Parameter Conditions`) na camada de autoria e execução.

## ADDED Requirements

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
