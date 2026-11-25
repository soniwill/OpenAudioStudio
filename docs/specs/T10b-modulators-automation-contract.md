# T10b — Moduladores (RANDOM, AHDSR) e Curvas de Automação

Este documento especifica os tipos, estruturas, algoritmos e critérios de aceitação para a **T10b-a** e **T10b-b** no OpenAudioEngine.

## 1. Escopo e Objetivos

* **Moduladores (`Modulator`):**
  * `RANDOM`: variação aleatória no momento de disparo de instrumentos ou eventos para propriedades de Volume (até 80 dB), Pitch (até 48 semitons) e Pan (até 1.0).
  * `AHDSR`: envelope modulador avaliado amostra-a-amostra na thread de áudio como função pura do `dsp_clock` com estágios:
    * Attack (`attack_ms`)
    * Hold (`hold_ms`)
    * Decay (`decay_ms`)
    * Sustain (`sustain` $0.0 \dots 1.0$)
    * Release (`release_ms`, disparado via `stop(ALLOW_FADEOUT)` ou `key_off`)
* **Curvas de Automação (`Automation` e `EffectAutomation`):**
  * Curvas lineares e não-lineares compostas por 1 a 32 pontos (`CurvePoint`), com ordenação estrita em `x`.
  * Formas de segmento (`CurveShape`):
    * `LINEAR`: interpolação $u = t$
    * `HOLD`: retém o valor do ponto anterior até o próximo ponto
    * `EASE_IN`: aceleração $u = t^2$
    * `EASE_OUT`: desaceleração $u = 1 - (1 - t)^2$
    * `S_CURVE`: curva suave $u = t^2 (3 - 2t)$
  * Destinos de automação:
    * Volume (em dB, onde $\le -80\text{ dB} = 0.0$ linear exato)
    * Pitch (em semitons)
    * Pan (em $[-1.0, +1.0]$)
    * Efeitos de Bus (`EffectAutomation`): índice e parâmetro de efeito em bus específico, com cache de época para miss.

---

## 2. Critérios de Aceitação

1. **AHDSR Audível e Decaimento:** Release de AHDSR decai no tempo configurado $\pm 10\%$ após `stop(ALLOW_FADEOUT)` ou `key_off`.
2. **Modulador RANDOM:** Variações aleatórias de volume, pitch e pan respeitam estritamente as faixas configuradas em 10.000 triggers com sementes determinísticas.
3. **Automação de Volume a -80 dB:** Quando a curva atinge $\le -80\text{ dB}$, a saída linear é exatamente zero (silêncio absoluto).
4. **Automação por Parâmetro (ex: RPM -> Pitch):** O pitch acompanha a curva de automação ao longo do parâmetro de controle verificado por taxa de zero-crossing.
5. **Automação de Linha do Tempo e Saltos de Loop:** Acompanha saltos de loop sem bursts ou descontinuidades indesejadas.
6. **Cache de Miss em Automação de Efeito:** Evita lookups de strings repetidos em buses inexistentes via época do sistema.

---

## 3. Validação e Evidências

* **Suíte de Testes:** `native/tests/modulators_automation.cpp` (cobrindo AHDSR, RANDOM em 10k amostragens, RPM->Pitch, Volume $\le -80\text{ dB}$ silêncio absoluto e EffectAutomation com invalidação de cache por época).
* **Gate Debug (`./scripts/build.ps1 -Config Debug -Full`):**
  * MSVC 19.43 / C++17 com `/W4 /WX` sem warnings ou erros.
  * 58 casos de teste, 63.176 asserções aprovadas (`RESULT: OK`).
* **Gate Release (`./scripts/build.ps1 -Config Release -Full`):**
  * Build otimizado sem warnings.
  * 100% dos testes aprovados (`RESULT: OK`).
* **Invariantes de Tempo Real:** Avaliação amostra-a-amostra do AHDSR no `StripNode` sem alocação dinâmica. Operações protegidas pelo `HeapGuard`.
