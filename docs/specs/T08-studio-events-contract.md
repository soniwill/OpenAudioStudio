# T08 — Modelo de Eventos Studio, Instâncias e Instrumentos

Este documento especifica o contrato, arquitetura interna, máquina de estados e API pública da **T08** no OpenAudioEngine.

## 1. Escopo e Objetivos

* **Modelo Imutável em Memória:** Estruturas `ModelSet`, `EventDescriptionModel`, `Track`, `Instrument`, `PlaylistEntry` e `Guid`. Uma vez construído via builder ou lido de banco, o modelo é estritamente imutável e thread-safe para leitura concorrente.
* **Builders C++ Fluentes:** `ModelSetBuilder` e `EventBuilder` para compor eventos, faixas, instrumentos e playlists diretamente em testes ou na autoria pré-compilação de bancos.
* **Gerador Aleatório Deterministico:** `Pcg32` em `native/src/common/pcg32.h` com suporte a seeds fixas para testes reproduzíveis.
* **Tipos de Instrumentos:**
  1. `SINGLE`: reproduz um som (sample index ou som pré-carregado).
  2. `MULTI`: seleciona entradas de playlist com modos `RANDOM` (ponderado), `RANDOM_NO_REPEAT` (sem repetições consecutivas), `SEQUENTIAL` (ordem circular) e `SHUFFLE` (permutação exaustiva por ciclo), em escopo `GLOBAL` (compartilhado) ou `INSTANCE` (isolado por instância).
  3. `EVENT`: instanciação aninhada de evento até profundidade 8.
  4. `COMMAND`: envio de comando para outro evento ou snapshot (`START_EVENT`, `STOP_EVENT`, `SNAPSHOT_ON`, `SNAPSHOT_OFF`).
  5. `SILENCE`: avanço de tempo sem geração de áudio.
* **Instâncias de Eventos (`oae_event_instance`):**
  * Máquina de estados: `STARTING`, `PLAYING`, `SUSTAINING`, `STOPPING`, `STOPPED`.
  * Limite `max_instances` e políticas de roubo:
    * `OLDEST`: interrompe a instância ativa mais antiga.
    * `QUIETEST`: interrompe a instância de menor ganho efetivo.
    * `NONE`: descarta o disparo da nova instância quando o limite for atingido, retornando `OAE_OK`.
  * `cooldown_ms`: se disparado dentro da janela desde o último disparo bem-sucedido daquele evento, descarta o trigger e retorna `OAE_OK`.
  * Gestão de parâmetros básicos de instância: volume, pitch e pan combinados com os valores do evento.
* **API C Pública:**
  * 17 funções exportadas: `oae_studio_get_event`, `oae_studio_play_one_shot`, `oae_event_description_create_instance`, `oae_event_description_get_length_ms`, `oae_event_description_is_oneshot`, `oae_event_description_get_instance_count`, `oae_event_instance_start`, `oae_event_instance_stop`, `oae_event_instance_release`, `oae_event_instance_get_playback_state`, `oae_event_instance_get_description`, `oae_event_instance_set_volume`, `oae_event_instance_get_volume`, `oae_event_instance_set_pitch`, `oae_event_instance_get_pitch`, `oae_event_instance_set_pan`, `oae_event_instance_get_pan`.

---

## 2. Estrutura de Dados e Invariantes

### 2.1. Guid e Identificação
O identificador universal `Guid` possui 16 bytes (`hi`, `lo`). Quando não especificado explicitamente, deriva deterministicamente do path do evento via FNV-1a 128-bit:
```cpp
struct Guid {
    uint64_t hi = 0;
    uint64_t lo = 0;
    bool operator==(const Guid& o) const noexcept { return hi == o.hi && lo == o.lo; }
    bool operator!=(const Guid& o) const noexcept { return !(*this == o); }
};
```

### 2.2. Modos de Playlist e Escopo
* `RANDOM`: seleciona uma entrada proporcionalmente ao seu peso (`weight / total_weight`).
* `RANDOM_NO_REPEAT`: como `RANDOM`, mas a entrada imediatamente anterior é excluída da seleção caso existam duas ou mais entradas na playlist.
* `SEQUENTIAL`: avança monotonicamente no vetor de entradas em anel (`(idx + 1) % size`).
* `SHUFFLE`: embaralha os índices no início de cada ciclo usando PCG32 e consome todos até esgotar antes de novo sorteio.
* **Escopo:**
  * `GLOBAL`: o estado de histórico do playlist é compartilhado por todas as instâncias do evento.
  * `INSTANCE`: cada instância mantém seu próprio estado de playlist.

### 2.3. Cooldown e Limite de Instâncias
* Quando `cooldown_ms > 0`: o sistema registra o timestamp do último disparo de cada `EventDescriptionModel`. Se uma chamada a `oae_event_instance_start` ou `oae_studio_play_one_shot` ocorrer com intervalo menor que `cooldown_ms`, o disparo é silenciado/ignorado e a função retorna `OAE_OK`.
* Quando `max_instances > 0`: o número de instâncias com estado diferente de `STOPPED` não pode exceder `max_instances`. Se um novo `start()` for chamado:
  * Se `steal == OLDEST`, a instância mais antiga passa para `STOPPED` (ou `STOPPING` se `stop_fadeout_ms > 0`).
  * Se `steal == QUIETEST`, a instância de menor ganho efetivo é finalizada.
  * Se `steal == NONE`, a nova instância é imediatamente finalizada sem tocar, mantendo exatamente `max_instances` instâncias ativas.

---

## 3. Exemplo de Uso na API C

```c
#include <oae_audio.h>

oae_result play_footstep(oae_system system)
{
    oae_event_description desc = 0;
    oae_result res = oae_studio_get_event(system, "event:/footstep", &desc);
    if (res != OAE_OK) return res;

    oae_event_instance inst = 0;
    res = oae_event_description_create_instance(desc, &inst);
    if (res != OAE_OK) return res;

    oae_event_instance_set_volume(inst, 0.8f);
    oae_event_instance_start(inst);

    /* Em cada frame do jogo: */
    oae_system_update(system);

    /* Ao encerrar: */
    oae_event_instance_stop(inst, OAE_STOP_ALLOW_FADEOUT);
    oae_event_instance_release(inst);
    return OAE_OK;
}
```

---

## 4. Critérios de Aceitação e Validação

1. **Multi-track:** Um evento com 2 faixas inicia sons em ambas as faixas simultaneamente.
2. **Precisão de Agendamento:** Instrumento com `offset_ms = 250` inicia com precisão de frames em relação ao relógio DSP (`250 ms * sample_rate / 1000`).
3. **Distribuição Ponderada:** Em 10.000 picks com seed fixa, as proporções convergem com tolerância de $\pm 5\%$.
4. **Sem Repetição:** `RANDOM_NO_REPEAT` nunca repete o mesmo item em duas chamadas sucessivas.
5. **Shuffle:** `SHUFFLE` consome todos os itens uma única vez por rodada antes de repetir qualquer item.
6. **Stealing e Cooldown:** `max_instances = 3` garante que nunca haja mais de 3 instâncias ativas simultaneamente em todos os 3 modos de steal. `cooldown = 100ms` descarta chamadas subsequentes dentro da janela retornando `OAE_OK`.
7. **Zero alocações na thread de áudio:** Toda a lógica de eventos executa na game thread (`oae_system_update` e API de instâncias), sem alocações durante `oae_system_render`.
