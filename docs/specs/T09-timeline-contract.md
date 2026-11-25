# T09 — Lógica de Linha do Tempo e Callbacks

> [!NOTE]
> Os requisitos normativos vigentes da capacidade `timeline-logic` estão migrando para [`openspec/specs/timeline-logic/spec.md`](../../openspec/specs/timeline-logic/spec.md). Consulte o spec OpenSpec para contratos de lógica de timeline, transições e condições.

Este documento define a especificação do contrato, estruturas, escalonamento e despacho de callbacks para a **T09** no OpenAudioEngine.

## 1. Escopo e Objetivos

* **Itens de Lógica (`LogicItem`):**
  * `MARKER`: ponto nomeado na timeline que gera callbacks `OAE_EVENT_CALLBACK_MARKER` com `dsp_clock` exato.
  * `TEMPO`: define BPM e fórmula de compasso (`ts_upper / ts_lower`), gerando callbacks `OAE_EVENT_CALLBACK_BEAT` sincronizados a cada batida.
  * `LOOP_REGION`: delimita um intervalo `[position_ms, end_ms]`. Suporta `repeat_count` contável ou infinito (`-1`), e `until_key_off` (sustenta até que `oae_event_instance_key_off` seja chamado).
  * `SUSTAIN_POINT`: ponto na timeline onde a reprodução pausa/espera até `key_off`.
  * `TRANSITION_MARKER` / `TRANSITION_REGION`: transição para um marcador alvo com quantização `IMMEDIATE`, `BEAT` ou `BAR` e tempo de `crossfade_ms`.
* **Fila de Callbacks Segura:**
  * Capacidade fixa de 4096 eventos para despacho estritamente na game thread durante `oae_system_update`.
  * Reserva de emergência: acima de 3072 itens, eventos secundários (`SOUND_PLAYED`, etc.) podem ser descartados se necessário, mas eventos críticos (`STARTED`, `STOPPED`, `STOLEN`) nunca são descartados.
  * Callbacks **nunca** executam na thread de áudio.
* **API C:**
  * `oae_event_instance_set_callback(instance, callback, mask, user_data)`
  * `oae_event_instance_key_off(instance)`
  * `oae_event_instance_get_timeline_position_ms(instance, out_position_ms)`
  * `oae_event_instance_set_timeline_position_ms(instance, position_ms)`

---

## 2. Invariantes de Linha do Tempo e Cálculo de Batidas

O cálculo de frames de batida multiplica diretamente pelo índice `n` (0-based) a partir do início da seção de tempo, evitando qualquer acúmulo de erro numérico:
$$\text{frames\_per\_beat} = \text{sample\_rate} \times \left(\frac{60.0}{\text{BPM}}\right) \times \left(\frac{4}{\text{ts\_lower}}\right)$$
$$\text{beat\_frame}(n) = \text{section\_start\_frame} + \text{round}(n \times \text{frames\_per\_beat})$$

Para 120 BPM em 4/4 a 48 kHz: $\text{frames\_per\_beat} = 48000 \times 0.5 = 24000$ frames exatos.

---

## 3. Critérios de Aceitação

1. **Callback MARKER:** Disparado com `dsp_clock` exatamente correspondente ao offset da posição em frames.
2. **Callback BEAT:** A 120 BPM em 4/4 a 48 kHz, batidas ocorrem com espaçamento rigoroso de 24.000 frames.
3. **Quantização BAR:** Transição configurada para barra salta exatamente no início da barra subsequente.
4. **Sustain e Key-Off:** Em uma região de loop com `until_key_off`, a instância continua repetindo até a chamada de `oae_event_instance_key_off`; após o key-off, a iteração atual termina seu curso até o fim da região e prossegue para a timeline subsequente.
5. **Thread Safety:** Nenhuma invocação de callback ocorre no contexto da thread de render de áudio.
