# modulation-automation Specification

## Purpose

Define a especificação normativa de moduladores (RANDOM e AHDSR) e curvas de automação dirigidas por parâmetros em eventos, faixas, instrumentos e efeitos de bus.

## Requirements

### Requirement: Autoria — Moduladores RANDOM e AHDSR em eventos e instrumentos
O `AuthoringModel` MUST permitir adicionar, editar e remover moduladores `random` (alvos `volume`, `pitch`, `pan`) e `ahdsr` em eventos e instrumentos suportados, respeitando os limites do schema `T10b` (`volume <= 80 dB`, `pitch <= 48 st`, `pan <= 1.0`, tempos `>= 0 ms` e `sustain` em `[0.0, 1.0]`).

#### Scenario: Adição e edição de modulador AHDSR com limites válidos
- **WHEN** um modulador `ahdsr` é anexado a um evento ou instrumento e tem `attack_ms`, `hold_ms`, `decay_ms`, `sustain`, `release_ms` ou `release_on_key_off` ajustados
- **THEN** o `AuthoringModel` normaliza os valores para o domínio válido e persiste o objeto em `modulators` com suporte a Undo/Redo

#### Scenario: Adição e edição de modulador RANDOM por alvo
- **WHEN** um modulador `random` é anexado com `target` em `volume`, `pitch` ou `pan`
- **THEN** o `AuthoringModel` restringe `range` ao limite do alvo escolhido (`80`, `48` ou `1.0`) e persiste em `modulators`

### Requirement: Autoria — Curvas de automação multiponto por parâmetro
O `AuthoringModel` MUST permitir criar, editar e remover curvas de automação dirigidas por parâmetros locais ou globais (`volume`, `pitch`, `pan` e `effect_automations`), mantendo de `1` a `32` pontos ordenados estritamente em `x` e selecionando a forma de segmento entre `LINEAR`, `HOLD`, `EASE_IN`, `EASE_OUT` e `S_CURVE`.

#### Scenario: Edição de pontos e forma de segmento em curva por parâmetro
- **WHEN** o usuário adiciona, move ou remove pontos em uma automação ligada a um parâmetro local ou global e escolhe a forma `S_CURVE`
- **THEN** os pontos permanecem ordenados por `x` dentro do limite de `1..32` pontos e a forma selecionada é persistida no projeto para compilação via `oae_bankc`
