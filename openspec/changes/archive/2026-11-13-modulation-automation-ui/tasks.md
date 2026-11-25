# Tasks

## 1. Camada Modelo de Autoria (`studio/Source/Model`)

- [x] 1.1 Adicionar helpers tipados no `AuthoringModel` para adicionar, atualizar e remover moduladores `random` e `ahdsr` em Eventos e Instrumentos (normalizando `range` por alvo, tempos `>= 0 ms` e `sustain` em `[0.0, 1.0]` com Undo/Redo) — verificar via smoke em `studio/Source/Main.cpp`.
- [x] 1.2 Adicionar helpers tipados no `AuthoringModel` para criar, editar pontos (`1..32` ordenados em `x`), alterar forma de segmento (`LINEAR`, `HOLD`, `EASE_IN`, `EASE_OUT`, `S_CURVE`) e remover automações por parâmetro em Eventos/Faixas e `effect_automations` (com Undo/Redo) — verificar via smoke em `studio/Source/Main.cpp` e compilação limpa via `oae_bankc`.

## 2. Camada Studio UI (`studio/Source/UI`)

- [x] 2.1 Criar o componente de gráfico interativo de envelope AHDSR (`AhdsrCurveEditor`) e integrar a seção **Modulation** (`RANDOM` + `AHDSR`) no `InspectorPanel` para Eventos e Instrumentos com gestos de Undo/Redo — verificar via build e smokes Release do Studio.
- [x] 2.2 Criar o componente editor de curvas por parâmetro (`AutomationCurveEditor`) e integrá-lo no `InspectorPanel` para automações de Evento/Faixa (`volume`, `pitch`, `pan`) e `effect_automations` de Bus com gestos de Undo/Redo — verificar via build e smokes Release do Studio.

## 3. Documentação e Gate de Verificação

- [x] 3.1 Atualizar o Capítulo 6 do manual offline (`docs/manual/chapters/06-inspetor-e-modulacao.html`) documentando a seção **Modulation** (`RANDOM`, `AHDSR`) e o editor de curvas por parâmetro (`T10b`) com passos reproduzíveis.
- [x] 3.2 Executar `scripts/verify.ps1 -Layer studio` e `scripts/verify.ps1 -Layer docs` após a última edição, validar `openspec validate "modulation-automation-ui" --strict` e registrar os resultados em `docs/ai/memory.md`.
