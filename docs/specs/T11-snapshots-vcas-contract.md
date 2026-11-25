# T11 — Snapshots & VCAs Contract

Este documento especifica a arquitetura, modelos de dados, algoritmos de composição e critérios de aceitação para **Snapshots** e **VCAs** (Voltage-Controlled Amplifiers) no OpenAudioEngine.

---

## 1. Escopo e Objetivos

### 1.1 Snapshots (`snapshot:/path`)
* Snapshots são modelados conceitualmente como descrições de eventos com `kind = EventKind::Snapshot`.
* Podem ser instanciados e controlados via API de eventos padrão:
  * `oae_studio_get_event(sys, "snapshot:/path", &desc)`
  * `oae_event_description_create_instance(desc, &inst)`
  * `oae_event_instance_start(inst)`
  * `oae_event_instance_stop(inst, mode)`
  * `oae_event_instance_set_parameter_by_name(inst, "intensity", value, ...)` (quando configurado)
* **Propriedades do Snapshot:**
  * `snapshot_mode`: `OVERRIDE` ou `BLENDING`.
  * `priority`: inteiro de prioridade (usado para desempate e ordem de resolução de OVERRIDE).
  * `fade_in_ms`: tempo de fade-in ao iniciar.
  * `stop_fadeout_ms`: tempo de fade-out ao parar com `OAE_STOP_ALLOW_FADEOUT`.
  * `intensity_param`: nome ou índice do parâmetro de intensidade (`0..100%`, default 100%).
  * `targets`: lista de alvos (`SnapshotTarget`):
    * `bus_path`: caminho do bus alvo (ex: `"bus:/music"`).
    * `param_id`: `-1` para volume relativo do bus em dB; `>= 0` para ID do parâmetro de efeito.
    * `position`: `OAE_EFFECT_POST_FADER` (0) ou `OAE_EFFECT_PRE_FADER` (1).
    * `index`: índice do efeito no bus.
    * `value`: valor no snapshot (dB delta para volume, ou valor do parâmetro do efeito).

### 1.2 VCAs (`vca:/path`)
* VCAs controlam ganhos agregados sobre grupos de buses (e eventos):
  * `oae_vca`: tipo de handle de 64-bit no pool do Runtime (`tag = 9`).
  * `oae_studio_get_vca(sys, "vca:/path", &vca)`
  * `oae_vca_set_volume(vca, volume)`: define ganho linear ($[0.0, 1.0]$).
  * `oae_vca_get_volume(vca, &volume)`: obtém ganho linear configurado.
* **Invariante Fundamental do VCA:**
  * O fader do bus (`oae_bus_get_volume`) permanece rigorosamente inalterado.
  * O multiplicador de ganho do VCA afeta o ganho efetivo composto (`Bus::effective_gain()` ou no `StripNode`/DSP do bus).
  * Múltiplos VCAs controlando o mesmo bus combinam multiplicativamente.

---

## 2. Algoritmo de Resolução de Snapshots

1. A cada atualização (`System::update_instances` / `update`), o sistema coleta todos os snapshots ativos:
   * Calcula o fator de envelope de fade do snapshot: $F_{fade}(t) \in [0.0, 1.0]$.
   * Multiplica pela intensidade parametrizada: $I = (intensity\_pct / 100.0) \times F_{fade}$.
2. Ordena os snapshots ativos por prioridade (menor para maior prioridade).
3. Para cada alvo (`bus_path`, propriedade):
   * Se modo `OVERRIDE`: o snapshot de maior prioridade ativo que define aquele alvo assume o controle, interpolando linearmente entre o valor base (0 dB delta ou valor original do parâmetro) e o valor alvo com base em $I$.
   * Se modo `BLENDING`: os deltas em dB somam ponderados por seus respectivos fatores $I_k$:
     $$\Delta dB = \sum_k (\Delta dB_k \times I_k)$$
4. Aplica os deltas acumulados no ganho de snapshot do bus ou parâmetros de efeito.

---

## 3. Critérios de Aceitação (AC)

1. Snapshot `"DuckMusic"` reduz `bus:/music` em 9 dB a 100% de intensidade e 4.5 dB a 50% de intensidade.
2. Dois snapshots `OVERRIDE` concorrentes resolvem por prioridade.
3. Parar snapshot com fade-out restaura progressivamente os valores originais.
4. VCA a `0.5` reduz a amplitude / RMS pela metade mantendo `oae_bus_get_volume` inalterado.
5. Gates Debug e Release com zero warnings (`/W4 /WX`) e 100% dos testes aprovados.

---

## 4. Validação e Evidências

* **Suíte de Testes:** `native/tests/snapshots_vcas.cpp`.
  * Cobertura de escala de volume por VCA e preservação estrita do fader (`bus_vol == 0.8f` imutável, RMS reduzido em 50% com `vca_vol == 0.5f`).
  * Cobertura de atenuação relativa em dB do snapshot `"DuckMusic"` a 100% (-9.0 dB) e a 50% de intensidade (-4.5 dB) e restauração bit a bit do ganho base.
  * Cobertura de múltiplos snapshots `OVERRIDE` concorrentes com desempate e prevalência estrita por prioridade (`priority 200 > 100`).
  * Precedência universal de erros da API C de VCA (`OAE_ERR_INVALID_PARAM`, `OAE_ERR_INVALID_HANDLE`, `OAE_ERR_NOT_FOUND`).
* **Gate Debug (`./scripts/build.ps1 -Config Debug -Full`):**
  * 62 casos de teste, 63.218 asserções aprovadas com `RESULT: OK`.
* **Gate Release (`./scripts/build.ps1 -Config Release -Full`):**
  * Compilação otimizada com zero avisos (`/W4 /WX`), 100% dos testes aprovados (`RESULT: OK`).
* **Invariantes em Tempo Real:** Zero alocações no caminho de áudio (`HeapGuard`), atenuação aplicada via ramps sem pops ou fader drift.
