# Proposal

## Why

O motor nativo e o compilador `oae_bankc` já suportam moduladores `RANDOM` e `AHDSR` e curvas de automação multiponto por parâmetro (`Automation` e `EffectAutomation` conforme `T10b`), mas o OpenAudio Studio ainda não oferece interface para criar, editar e remover moduladores nem um editor de curvas dirigidas por parâmetro no `InspectorPanel`, obrigando a edição manual de `project.json`.

## What Changes

- **Seção `Modulation` no `InspectorPanel` (`RANDOM` + `AHDSR`)**: permitir adicionar, editar e remover moduladores em Eventos e Instrumentos (`single`, `multi`, `scatterer`, `programmer`), com controles para `random` (`volume`, `pitch`, `pan`) e `ahdsr` (`attack_ms`, `hold_ms`, `decay_ms`, `sustain`, `release_ms`, `release_on_key_off`), acompanhados de gráfico interativo da curva AHDSR e Undo/Redo por gesto.
- **Editor de Curvas de Automação por Parâmetro no `InspectorPanel`**: permitir criar, editar e remover automações dirigidas por parâmetros locais ou globais (`volume`, `pitch`, `pan` e `effect_automations`), com edição de `1..32` pontos ordenados em `x` e seleção das 5 formas de segmento (`LINEAR`, `HOLD`, `EASE_IN`, `EASE_OUT`, `S_CURVE`).
- **Prévia, validação e manual**: garantir reconstrução da prévia (`PreviewRebuilder`), compilação limpa via `oae_bankc`, cobertura em smokes do Studio e atualização do Capítulo 6 do manual offline.
- **Exclusões**: não inclui moduladores `LFO` ou `Seek` (não existentes no runtime `T10b`), nem alterações no algoritmo DSP ou formato binário `.bank`, nem edição de modulação dinâmica específica de spawners de `Scatterer`.

## Capabilities

### New Capabilities
- `modulation-automation`: especifica os requisitos canônicos de modulação (`RANDOM`, `AHDSR`) e curvas de automação por parâmetro (`1..32` pontos, 5 formas de segmento e alvos `volume`/`pitch`/`pan`/`EffectAutomation`), migrando o escopo UI/autoria de `T10b` / Épico 12.

### Modified Capabilities
- `studio-shell`: adiciona os requisitos de interface no `InspectorPanel` para a seção de modulação (`RANDOM`, `AHDSR` com gráfico interativo) e o editor de curvas de automação por parâmetro com Undo/Redo por gesto.

## Impact

- **Camada Modelo (`studio/Source/Model/`)**: `AuthoringModel` (helpers tipados para `modulators` em evento/instrumento e `automations` / `effect_automations` com validação de invariantes e Undo/Redo).
- **Camada Studio UI (`studio/Source/UI/`)**: `InspectorPanel` + componentes auxiliares de gráfico AHDSR e editor de curvas de automação por parâmetro; smokes em `studio/Source/Main.cpp`.
- **Camada Documentação (`docs/manual/`)**: atualização do Capítulo 6 (`docs/manual/chapters/06-inspetor-e-modulacao.html`).
- **Camada Runtime/Banco (`native/`)**: sem alterações de DSP, ABI C ou formato `.bank`.
