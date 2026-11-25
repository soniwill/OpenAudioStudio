# Proposta: Parâmetros por Parameter Sheet, Modificadores (Velocity/Hold) e User Properties

## Motivação
A auditoria de paridade com o benchmark FMOD Studio 2.04 / Wwise apontou lacunas em `LOG-02` (Parameter Sheets não expostos no schema JSON, `oae_bankc` e UI), `PAR-01` (parâmetros locais e kinds `Discrete`/`Labeled` + `Seek Speed` sem edição completa no Studio), `PAR-03` (ausência dos modificadores `Velocity` e `Hold value during playback`) e `EVT-01` (edição visual de `user_props`, tags e notes no Studio). Embora o runtime de `EventInstance::evaluate_parameter_sheets()` já avalie `sheet_parameter` e `[param_min, param_max]`, a pipeline de autoria e compilação `.bank` não transporta esses campos.

## Escopo
1. Expor `sheet_parameter`, `param_min` e `param_max` em `project-v1.schema.proposed.json`, `oae_bankc`, formato `.bank` (chunk opcional `PSHT`), `AuthoringModel` e `InspectorPanel`.
2. Implementar `velocity` (unidades por segundo) e `hold_during_playback` (`Read Only` durante reprodução ativa) em `ParameterDefinition`, `ParameterInstance`, `EventInstance`, formato `.bank` (chunk opcional `PMOD`), `project-v1.schema.proposed.json` e `oae_bankc`.
3. Completar edição no Studio (`AuthoringModel` e `InspectorPanel`) de parâmetros globais e locais (`continuous`, `discrete`, `labeled`, `seek_speed`, `seek_speed_down`, `velocity`, `hold_during_playback`) e de `user_props` / tags / notes em eventos.

## Exclusões
- Parâmetros Built-in 3D automáticos (`Distance`, `Direction`, `Elevation`, `EventConeAngle`, `Speed`) ficam para a Etapa 3 (`spatialization-3d-and-builtin-params`).
- Atribuição explícita multi-banco e telemetria live ficam para a Etapa 4.

## Capacidades Afetadas e Impacto por Camada
- `parameters` (nova capacidade de conceito): modificadores `velocity` e `hold_during_playback`, tipos `continuous`/`discrete`/`labeled` e `seek_speed`/`seek_speed_down` no runtime e autoria.
- `instruments`: autoria, validação e serialização de `sheet_parameter`, `param_min` e `param_max` para Parameter Sheets.
- `studio-shell`: edição completa de parâmetros globais/locais, faixas de Parameter Sheet e `user_props` (notes/tags/propriedades customizadas) no Inspector e `AuthoringModel`.
- Camadas impactadas: `native/src/core`, `native/src/studio`, `native/tools/bankc`, `docs/specs/project-v1.schema.proposed.json`, `studio/Source/Model`, `studio/Source/UI` e suites CTest + Studio smoke.
