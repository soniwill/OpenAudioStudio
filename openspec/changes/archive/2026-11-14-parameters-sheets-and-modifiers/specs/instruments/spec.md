# Spec Delta

## ADDED Requirements

### Requirement: Autoria — Disparo de Instrumentos por Intervalo de Parameter Sheet
O schema `project.json` v1, o compilador `oae_bankc`, o formato `.bank` e a DAW (`AuthoringModel` e `InspectorPanel`) MUST suportar vincular instrumentos a um `sheet_parameter` e intervalo `[param_min, param_max]`, preservando o disparo e liberação em runtime via `evaluate_parameter_sheets`.

#### Scenario: Compilação e disparo por Parameter Sheet em banco compilado
- **WHEN** um instrumento declara `sheet_parameter`, `param_min` e `param_max` dentro do domínio do parâmetro referenciado e o projeto é compilado por `oae_bankc`
- **THEN** o banco `.bank` gerado restaura `sheet_parameter`, `param_min` e `param_max` no `ModelSet`
- **AND** o evento reproduz o instrumento apenas quando o parâmetro entra no intervalo configurado

#### Scenario: Rejeição de Parameter Sheet com parâmetro inexistente ou intervalo inválido
- **WHEN** um instrumento referencia um `sheet_parameter` inexistente ou declara `param_min > param_max` fora do domínio `[min, max]` do parâmetro
- **THEN** o compilador `oae_bankc` rejeita o projeto com diagnóstico explícito
