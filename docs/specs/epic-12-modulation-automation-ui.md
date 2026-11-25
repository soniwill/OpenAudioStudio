# Épico 12 — Interface de Modulação (AHDSR, RANDOM) e Curvas de Automação por Parâmetro

> **Status:** Especificação proposta (não implementada; requer autorização explícita para iniciar execução).

## Objetivo e escopo

Expor visualmente no **OpenAudio Studio** os recursos de modulação e automação já suportados pelo motor nativo e pelo compilador `oae_bankc` (especificados em [T10b-modulators-automation-contract.md](T10b-modulators-automation-contract.md) e no schema [project-v1.schema.proposed.json](project-v1.schema.proposed.json)), eliminando a necessidade de editar `project.json` manualmente.

### Escopo
- **MOD-UI-01 — Seção de Modulação no `Property Inspector` (`RANDOM` + `AHDSR`):**
  - Adicionar seção colapsável **Modulation** no `InspectorPanel` para Eventos e Instrumentos (`single` / `multi`), permitindo anexar, editar e remover moduladores em `volume`, `pitch` e `pan`.
  - **Modulador `RANDOM`:** slider de alcance simétrico (`± dB` até `80 dB`, `± st` até `48 st`, `± pan` até `1.0`) e botão de remoção, persistindo no array `modulators` com undo/redo via `AuthoringModel`.
  - **Envelope `AHDSR`:** controles para `attack_ms`, `hold_ms`, `decay_ms`, `sustain` (`0.0..1.0` / `0..100%`) e `release_ms`, acompanhados de um **gráfico interativo da curva AHDSR** (arraste direto dos pontos A, H, D, S, R e marcação visual de `Key Off / Release`).
- **MOD-UI-02 — Editor de Curvas de Automação por Parâmetro (`AutomationCurveEditor`):**
  - Permitir criar e editar curvas de automação dirigidas por parâmetros locais ou globais (ex.: `RPM -> Pitch`, `Combate -> Volume`, `Submerso -> Cutoff` de efeito em bus) além da automação temporal existente na subfaixa `A` da Timeline.
  - Suportar edição de pontos (`1..32` pontos ordenados em `x`), seleção de forma de segmento (`LINEAR`, `HOLD`, `EASE_IN`, `EASE_OUT`, `S_CURVE`) e escolha de destino (`volume`, `pitch`, `pan`, `EffectAutomation`).
- **MOD-UI-03 — Prévia ao Vivo, Validação e Documentação:**
  - Garantir que alterações em moduladores e curvas disparem reconstrução do preview (`PreviewRebuilder`), passem sem erros no `oae_bankc` e funcionem com `Key Off` / `Stop (ALLOW_FADEOUT)` no transporte.
  - Atualizar o Capítulo 6 do manual (`docs/manual/chapters/06-inspetor-e-modulacao.html`) com capturas reais da nova interface.

### Não escopo
- Alterações no algoritmo DSP ou no formato binário `.bank` (o suporte nativo de T10b já existe).
- Modulação LFO ou moduladores de efeitos não previstos no contrato T10b.

## Entradas, saídas e invariantes

- **Entradas:** seleção atual no `AuthoringModel` (Evento, Faixa, Instrumento ou Bus) e parâmetros locais/globais disponíveis no projeto.
- **Saídas:** mutações atômicas via `AuthoringModel::modify()` / gestos `begin_gesture()` + `end_gesture()`, persistidas em `project.json` no formato T10b (`modulators` e `automations` / `effect_automations`).
- **Invariantes:**
  - `RANDOM` sempre respeita os limites nativos (`volume <= 80 dB`, `pitch <= 48 st`, `pan <= 1.0`).
  - `AHDSR` mantém tempos `>= 0 ms` e `sustain` em `[0.0, 1.0]`.
  - `CurvePoint` mantém ordenação estrita em `x` (`1..32` pontos) e `y` dentro do domínio do destino.
  - Atualizações de UI durante arraste de sliders/pontos usam `begin_gesture()` / `end_gesture()` sem destruir componentes ativos na pilha de mouse.

## Tarefas e gates

| Tarefa | Entrega e critério de aceite |
| --- | --- |
| **MOD-UI-01 — AHDSR e RANDOM no Inspector** | Criar/editar/remover moduladores `RANDOM` e `AHDSR` em Evento e Instrumento pelo Inspector, com gráfico AHDSR interativo, undo/redo (`Ctrl+Z`/`Ctrl+Y`), preview audível (incluindo cauda de `Release` no `Key Off`) e compilação limpa no `oae_bankc`. |
| **MOD-UI-02 — Editor de Curvas por Parâmetro** | Criar/editar/remover automações por parâmetro (`Volume`, `Pitch`, `Pan` e `EffectAutomation`) com seleção dos 5 tipos de segmento (`LINEAR`, `HOLD`, `EASE_IN`, `EASE_OUT`, `S_CURVE`), respeito a `1..32` pontos ordenados e validação em preview/banco. |
| **MOD-UI-03 — Smoke, Paridade e Manual** | Ampliar `--smoke-pro-ui` / `--smoke-authoring` para cobrir criação, edição, undo/redo e compilação de `AHDSR`, `RANDOM` e curva por parâmetro; atualizar capturas e texto do Cap. 6 do manual. |

## Exemplo de uso esperado (fluxo do usuário)

1. Selecionar um evento de música ou ambiente na Timeline.
2. No **Property Inspector**, clicar em **+ Add Modulation → AHDSR (Volume)** e ajustar `Attack = 300 ms`, `Sustain = 100%`, `Release = 800 ms` arrastando o gráfico ou os sliders.
3. Pressionar **Play** e depois **Key Off**: o som entra em fade-in de 300 ms e decai suavemente em 800 ms após o `Key Off`.
4. Em um parâmetro global/local (ex.: `Velocidade`), adicionar uma curva `S_CURVE` ligada ao `Pitch` do evento e mover o parâmetro durante a audição para ouvir a resposta contínua.

## Questões em aberto

- Decidir se o **Editor de Curvas por Parâmetro** ficará em uma subseção do **Property Inspector**, em uma nova aba do **Contextual Bottom Deck** ou integrado à subfaixa **`A` (Automation)** da Timeline.
