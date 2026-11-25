# Épico 9 — Expansão de Áudio Interativo Avançado (`T18` a `T25`)

> **Status:** Especificação base implementada (`T18` a `T25`) no motor nativo, API C, wrappers Unity/C# e testes automatizados.

## 1. Objetivo e Escopo

Consolidar os contratos arquiteturais, invariantes de tempo real, extensões de schema/API e critérios de aceitação para as tarefas `T18` a `T25` do backlog:

- **T18:** Audio Sends and Returns (`return:/...`, pré/pós-fader, detecção de ciclos, remoção lock-free).
- **T19:** Per-Event Track Mixing and Effects (sub-mixer por instância, roteamento entre faixas, cadeia DSP por faixa e cauda no `stop`).
- **T20:** Asynchronous Instruments (`async` em `SINGLE`/`MULTI` através de saltos de timeline e políticas de retrigger/overlap).
- **T21:** Parameter Sheets and Region Triggering (abas guiadas por parâmetro e disparo/liberação por região).
- **T22:** Programmer Instruments and Audio Tables (resolução dinâmica por chave em runtime com callbacks AOT-safe na game thread).
- **T23:** Unity Timeline Integration (`PlayableDirector`, scrubbing, preview em editor, automação de parâmetros e parada no fim do clipe).
- **T24:** Unity Addressables and AssetBundles (providers assíncronos com contagem de referências para `.bank` + `.bank.catalog.json`).
- **T25:** Nested/Overlapping Loops & Event Pitch Timing (prioridade explícita de loops e acoplamento opcional de velocidade do playhead ao pitch do evento).

### Não Escopo

- Portabilidade de plataforma (Linux, iOS, WebGL), transcodificação por plataforma no `oae_bankc`, profiler em tempo real e espacialização 3D completa (Épico 10 / Milestones 2 e 3).
- Protocolo de Live-Linking (Épico 8).

---

## 2. Regras Transversais e Invariantes

1. **Zero alocações na thread de áudio:** Toda renderização em `BusDsp`, `StripNode`, `Channel`, sub-mixers de instância (`T19`), sends/returns (`T18`) e avaliação de regiões (`T21`) deve operar sem `new`/`delete`/`malloc`/`free` no caminho de áudio (validável com `HeapGuard`).
2. **Thread de callbacks:** Callbacks de `T22` (Programmer Instruments) e quaisquer notificações de ciclo de vida devem ser despachados exclusivamente na game thread (`oae_system_update`), com trampolins `[AOT.MonoPInvokeCallback]` compatíveis com IL2CPP na Unity.
3. **Compatibilidade de ABI e formato `.bank`:**
   - A adição de novas funções na API C (`native/include/oae_audio.h`) deve atualizar `oae_get_struct_sizes` caso novas structs públicas sejam introduzidas e manter `OaeNative.cs` sincronizado.
   - Extensões ao formato `.bank` devem preservar alinhamento de 16 bytes, verificação CRC-32 por chunk e compatibilidade com bancos v1 existentes (rejeitando dados malformados com `OAE_ERR_FORMAT`).
4. **Invariante de ganho e mute:** Ganho $\le -80\text{ dB}$ (`db_to_linear`) produz zero exato (`0.0f`); mute zera a saída preservando o ganho configurado e a cauda quando aplicável.

---

## 3. Especificação por Tarefa

### T18 — Audio Sends and Returns

- **Escopo:** Permitir envios (*sends*) pré-fader (`OAE_EFFECT_PRE_FADER`) ou pós-fader (`OAE_EFFECT_POST_FADER`) com nível ajustável (linear/dB) de qualquer bus (`bus:/...` ou `return:/...`) para buses de retorno (`return:/<nome>`).
- **Entradas / Saídas:**
  - Modelo/Schema: definição de buses de retorno (`return:/reverb`, etc.) e lista de sends por bus (`target_bus`, `level`, `position`, `muted`).
  - Runtime (`Bus` / `BusDsp`): tabela lock-free de sends (usando padrão double-buffered `AudioSlots` já existente em `native/src/core/bus_dsp.h`), acumulando buffers de saída no bus de retorno antes do processamento DSP do retorno no mesmo bloco.
- **Invariantes e Erros:**
  - **Rejeição de ciclos:** Em tempo de criação/conexão (e validação no `oae_bankc`), qualquer caminho que forme ciclo no grafo de roteamento (ex.: `A -> B -> A` ou auto-send `A -> A`) deve ser rejeitado com `OAE_ERR_INVALID_PARAM` (ou erro de validação no compilador).
  - **Remoção lock-free:** Remover um send ou liberar um bus de retorno (`oae_bus_release`) desacopla o ponteiro/índice na snapshot seguinte via `AudioSlots` sem alocar nem bloquear a thread de áudio.
  - **Ordem topológica:** Buses de retorno devem ser processados após todos os buses que enviam sinal para eles no mesmo bloco de áudio (ou com ordenação topológica determinística mantida na game thread ao alterar a topologia).
- **Critérios de Aceitação:**
  - Envio pré-fader continua alimentando `return:/reverb` mesmo com o fader do bus origem em `0.0f` (`-inf`); envio pós-fader acompanha o fader e o mute do bus origem.
  - Tentativa de criar ciclo retorna `OAE_ERR_INVALID_PARAM` sem corromper o grafo.
  - `HeapGuard` aprova adição/ajuste/remoção de sends durante render contínuo.

### T19 — Per-Event Track Mixing and Effects

- **Escopo:** Dotar cada `EventInstance` de um sub-mixer dedicado para suas `Track`s, com fader/mute/pan por faixa, roteamento entre faixas da mesma instância e cadeia de efeitos DSP (`Effect`, até o limite por cadeia) por faixa.
- **Entradas / Saídas:**
  - Cada faixa de uma `EventInstance` possui seu nó de mixagem/DSP antes de somar na saída da instância (que então vai para o bus de destino do evento).
  - Roteamento entre faixas dentro do evento (ex.: faixa A -> faixa B ou faixa -> master do evento) com validação acíclica.
- **Invariantes e Erros:**
  - **Gerenciamento de cauda no `stop`:** Ao parar com `ALLOW_FADEOUT`, a instância permanece viva enquanto houver sinal/cauda ativa nos efeitos das faixas (reverb/delay), liberando vozes/handles apenas após o término da cauda ou `STOP_IMMEDIATE`.
  - **Sem alocação no render:** Buffers intermediários por faixa pré-alocados na criação da instância (ou pool pré-alocado no `System`).
- **Critérios de Aceitação:**
  - Efeito inserido na Track 1 de um evento afeta apenas a Track 1 dessa instância (sem vazar para a Track 2 ou outras instâncias).
  - `stop(ALLOW_FADEOUT)` preserva a cauda de delay/reverb da faixa até silenciar.

### T20 — Asynchronous Instruments

- **Escopo:** Suporte ao modo `async` em instrumentos `Single` e `Multi`, desacoplando o ciclo de vida do som disparado do cursor da timeline após o gatilho inicial.
- **Entradas / Saídas:**
  - Flag `async` no `Instrument` (modelo, schema JSON, `.bank` e UI do Inspector/Timeline).
  - Política configurável de retrigger/overlap quando o playhead retorna à região de um instrumento `async` que ainda está tocando (ex.: sobrepor nova voz, ignorar enquanto ativo, ou cortar a anterior).
- **Invariantes:**
  - Saltos de playhead (`LOOP_REGION`, `TRANSITION_MARKER`, `TRANSITION_REGION` ou seek) **não** interrompem instrumentos `async` já disparados.
  - `stop()` explícito da instância (ou `key_off` quando aplicável a AHDSR/loop) encerra normalmente as vozes `async` associadas.
- **Critérios de Aceitação:**
  - Instrumento `async` mais longo que a `LOOP_REGION` continua tocando sem corte na transição de volta ao início do loop.
  - Instrumento síncrono (padrão `async = false`) mantém o comportamento atual de recorte nos limites da região/salto.

### T21 — Parameter Sheets and Region Triggering

- **Escopo:** Permitir que um evento possua abas (*sheets*) cujo eixo horizontal é um parâmetro (`CONTINUOUS`, `DISCRETE` ou `LABELED`), além da timeline temporal.
- **Entradas / Saídas:**
  - Regiões de instrumento definidas por `[param_min, param_max]` em vez de `[offset_ms, offset_ms + length_ms]`.
  - A avaliação na atualização da instância compara o valor interpolado atual do parâmetro (`ParameterInstance`) com os intervalos das regiões.
- **Invariantes:**
  - Entrada na região (`prev` fora, `curr` dentro) dispara o instrumento; saída da região (`prev` dentro, `curr` fora) encerra/inicia release (AHDSR) do instrumento disparado por aquela região.
  - Compatível com `seek_speed` do parâmetro: a travessia gradual dispara e libera regiões na ordem em que o cursor interpolado cruza os limites.
- **Critérios de Aceitação:**
  - Evento com Parameter Sheet de `SurfaceType` ou `Speed` dispara exclusivamente os instrumentos cujas regiões contêm o valor atual e libera os anteriores ao sair da faixa.

### T22 — Programmer Instruments and Audio Tables

- **Escopo:** Novo tipo de instrumento (`InstrumentKind::Programmer`) cujo conteúdo sonoro é resolvido dinamicamente em runtime pela game engine através de uma chave de texto/tabela de áudio.
- **Entradas / Saídas:**
  - Tabela de áudio (mapeamento `key -> sample/asset` empacotável em banco ou diretório/manifesto).
  - Callback na API C para solicitação/liberação de som do Programmer Instrument (executado na game thread durante `oae_system_update`), retornando um `oae_sound` (ou identificador da tabela) para a instância reproduzir.
  - Suporte no wrapper C# (`EventInstance` / `OaeNative`) com `[AOT.MonoPInvokeCallback]`.
- **Invariantes e Erros:**
  - Nenhum callback de usuário é invocado na thread de áudio.
  - Se o callback não fornecer som válido ou a chave não existir, a instância não falha catastroficamente (trata como silêncio seguro ou registra erro controlado conforme contrato).
- **Critérios de Aceitação:**
  - Teste nativo e C# resolvendo chaves distintas (ex.: `"dlg_intro_pt"`, `"dlg_intro_en"`) em runtime no mesmo evento sem duplicar eventos no projeto.

### T23 — Unity Timeline Integration

- **Escopo:** Integração nativa com `UnityEngine.Timeline` (`PlayableDirector`) no pacote `unity/com.openaudio.core/`.
- **Entradas / Saídas:**
  - `OaeEventTrack` (`TrackAsset`), `OaeEventClip` (`PlayableAsset`) e `OaeEventPlayableBehaviour` (`PlayableBehaviour`).
  - Suporte a seleção de evento via `[EventRef]`, curvas de automação de parâmetros no clipe e política de parada ao fim do clipe (`AllowFadeout`, `Immediate`, `None`).
- **Invariantes:**
  - Scrubbing no Editor e preview fora de PlayMode não devem vazar instâncias nativas (`Release()` garantido em `OnPlayableDestroy` / `OnGraphStop`).
  - Dependência opcional/isolada para não quebrar builds caso o pacote `com.unity.timeline` não esteja presente (via `versionDefines` no `.asmdef`).

### T24 — Unity Addressables and AssetBundles

- **Escopo:** Carregamento assíncrono de pares `.bank` + `.bank.catalog.json` via `AssetBundle` e `Addressables` (quando habilitado via `versionDefines`).
- **Entradas / Saídas:**
  - Provider assíncrono com contagem de referências por banco/chave: múltiplas requisições simultâneas para o mesmo banco compartilham a carga e só chamam `Unload` quando a contagem de referências chega a zero.
  - Validação de CRC-32 contra o catálogo antes do carregamento no engine nativo.
- **Invariantes:**
  - Nunca descarregar um banco ou liberar o `TextAsset`/buffer subjacente enquanto `oae_studio_get_bank_loading_state` ainda estiver em `OAE_BANK_LOADING` ou houver referências ativas.

### T25 — Nested/Overlapping Loops & Event Pitch Timing

- **Escopo:**
  1. **Nested/Overlapping Loops:** Múltiplas `LOOP_REGION` aninhadas ou sobrepostas na timeline com campo explícito de prioridade (ou regra determinística de resolução) e respeito a condições/`iterations`/`until_key_off`.
  2. **Event Pitch Timing:** Modo opcional de pitch do evento em que alterar o pitch do evento (ou parâmetro/modulador de pitch no nível do evento) escala proporcionalmente a velocidade de avanço do playhead da timeline ($2^{\text{semitons}/12}$) em conjunto com o pitch acústico das vozes.
- **Invariantes:**
  - Modo padrão preserva o comportamento existente (pitch apenas acústico, sem alterar o tempo da timeline) para não quebrar eventos existentes.
  - Com `pitch_affects_timeline` ativo, um aumento de `+12` semitons dobra a taxa de avanço da timeline e o pitch acústico das vozes filhas.

---

## 4. Exemplo Mínimo de Uso (T18 — Sends e Returns em C)

```c
#include <oae_audio.h>

/* Exemplo conceitual do contrato proposto para T18 (sujeito à revisão da API final) */
oae_result setup_reverb_send(oae_system sys, oae_bus sfx, oae_bus* out_return)
{
    oae_result r = oae_bus_create_return(sys, "reverb", out_return);
    if (r != OAE_OK) return r;

    /* Envia -6 dB (~0.5f) pós-fader do bus SFX para return:/reverb */
    return oae_bus_add_send(sfx, *out_return, 0.5f, OAE_EFFECT_POST_FADER);
}
```

---

## 5. Questões Abertas (Aguardando Decisão Antes da Implementação)

Para evitar suposições não aprovadas (conforme `AGENTS.md`), os pontos abaixo devem ser confirmados antes de iniciar a implementação de cada tarefa correspondente:

1. **T18 (Sends/Returns):** Limite máximo de sends por bus (ex.: 8, alinhado a `BusDsp::chain_limit`?) e se `return:/` pode ser criado exclusivamente no nível raiz (`return:/<nome>`) ou hierárquico (`return:/ambience/cave`).
2. **T19 (Sub-mixer por Evento):** Limite máximo de faixas roteáveis/efeitos por faixa em uma `EventInstance` para pré-alocação sem heap no render.
3. **T20 (Async Instruments):** Política padrão de overlap quando `async = true` e o playhead reentra na região (sobrepor até `max_instances` vs. ignorar retrigger).
4. **T22 (Audio Tables):** Formato do arquivo/chunk de tabela de áudio (JSON externo indexado no catálogo vs. chunk dedicado no `.bank` com ou sem empacotamento inline dos samples).
5. **T25 (Nested/Overlapping Loops):** Regra de desempate quando duas regiões de loop sobrepostas têm a mesma prioridade numérica (ex.: menor extensão / mais interna vence, ou ordem no array).
