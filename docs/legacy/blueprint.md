# OpenAudioEngine (OAE) — Master Blueprint, Wire Specification & Autonomous Backlog

> **DOCUMENTO PRINCIPAL CONSOLIDADO — AVANÇO AUTOMÁTICO AUTORIZADO.**
> Este `_fix.md` é a fonte canônica de produto, arquitetura e backlog. O blueprint sem sufixo é referência histórica e não deve orientar novas implementações.
> Leia [AGENTS.md](AGENTS.md) e [regras de engenharia e operação](docs/ai/rules.md): portabilidade entre ferramentas de IA, economia de tokens, multiagente quando vantajoso, memória persistente, uso do modelo e esforço da sessão, SOLID quando necessário, documentação e exemplos para todas as funcionalidades.
> Comentários no código de produto somente para documentar classes e métodos; anotações explicativas nos exemplos deste documento são documentação, não autorização para copiá-las ao código. A definição de `db_to_linear` abaixo foi fornecida expressamente pelo usuário.
> O usuário autorizou o avanço automático até concluir a T07. Interrompa após validação e documentação da T07. A T08 aguarda nova instrução. Diante de incerteza, pare o trabalho afetado e pergunte; fora dela, atue autonomamente no escopo autorizado.

> **INSTRUCAO PARA O AGENTE DE IA (MODO AUTONOMO):**
> Este documento e a especificacao completa e auto-contida para construir o middleware de audio **OpenAudioEngine (`oae_audio`)** do zero em **C++17**.
> - **Padrao de Linguagem Unico:** C++17 (`CMAKE_CXX_STANDARD 17`) e C11 (`CMAKE_C_STANDARD 11`). Nao use recursos exclusivos do C++20.
> - **Ordem Estrita de Execucao:** Execute as tarefas exatamente na sequencia contigua: `T01` -> `T02` -> `T03` -> `T04` -> `T05` -> `T06` -> `T07` -> `T08` -> `T09` -> `T10a` -> `T10b-a` -> `T10b-b` -> `T11` -> `T12a` -> `T12b` -> `T13` -> `T14a` -> `T14b` -> `T14c` -> `T15a` -> `T15b` -> `T16a` -> `T16b` -> `T16c` -> `T16d` -> `T16e` -> `T16f` -> `T17` -> `MCP-01`..`MCP-05` -> `VU-01`..`VU-04` -> `V1-01`..`V1-06` -> `LIVE-01`..`LIVE-04` -> `T18`..`T25` -> Milestones 2 & 3.
> - **Decisao Arquitetural sobre a T16:** As tarefas `T16a` a `T16f` (Studio Authoring DAW) sao implementadas **diretamente como uma aplicacao desktop nativa em C++17 com JUCE 8.0.12** (anteriormente referenciadas como `JP01` a `JP06`). **Nao** crie nenhuma aplicacao de autoria usando Unity UI Toolkit.
> - **Gate Obrigatorio por Tarefa:** Nenhuma tarefa esta concluida ate que Debug e Release compilem com **zero warnings** (`/W4` no MSVC ou `-Wall -Wextra` no GCC/Clang) e todos os testes passem em modo headless (`OAE_INIT_NO_DEVICE`).

---

## 1. Visao Geral e Layout do Repositorio

Um middleware completo de audio interativo para jogos (equivalente open-source ao FMOD Studio / Wwise), composto por 4 camadas:
1. **Native Core & Studio Runtime (`oae_audio.dll` / `oae_audio_static.lib`):** Engine nativa em C++17 sobre `miniaudio`, exportando uma ABI C plana e estavel (`oae_audio.h`).
2. **Bank Compiler CLI (`oae_bankc.exe`):** Compilador de linha de comando em C++17 que valida um `project.json` v1 e publica atomicamente `.bank` + `.bank.catalog.json`.
3. **Unity Integration Package (`com.openaudio.core`):** Camada C# P/Invoke blittable, wrappers seguros, componentes de cena e seletor `[EventRef]`.
4. **OpenAudio Studio Desktop DAW (`studio/`):** Aplicacao desktop standalone para Windows x64 em C++17 + JUCE 8.0.12 para autoria visual de eventos, timeline multi-track, curvas de automacao, mixer, snapshots e exportacao de bancos.

### Estrutura de Diretorios
```text
CMakeLists.txt                 # Build raiz (C++17, C11, MSVC /W4 /permissive- /utf-8)
scripts/
  build.ps1                    # Script canonico de build e testes nativos
  build_studio.ps1             # Script de build e smoke tests da DAW JUCE (T16a-T16f)
  deploy_unity.ps1             # Copia a DLL Release para o pacote Unity (T15a)
  unity_test.ps1               # Executa testes EditMode/PlayMode da Unity em batchmode
native/
  include/oae_audio.h          # Cabecalho unico da API C publica (ABI estavel)
  src/
    api/                       # Wrappers extern "C" -> validacao + try/catch -> core/studio
    core/                      # Core Engine (System, Sound, Channel, Bus, StripNode, VoiceMgmt, fx/)
    studio/                    # Studio Engine (EventModel, EventInstance, Timeline, Parameters, Automation, Snapshots, BankReader/Writer)
    common/                    # Utilitarios (HandlePool, SpscQueue, Ahdsr, FadeCurve, Meter, Pcg32)
    miniaudio_impl.c           # Unidade de traducao C11 unica para miniaudio + stb_vorbis
  external/                    # Dependencias vendorizadas (miniaudio, stb_vorbis, doctest, nlohmann_json)
  tools/bankc/                 # CLI Compiler (oae_bankc: project.json -> .bank + .bank.catalog.json)
  tests/                       # Testes automatizados doctest (*.cpp auto-globbed via CONFIGURE_DEPENDS)
studio/                        # Aplicacao Desktop DAW em C++17 / JUCE 8.0.12 (T16a-T16f)
unity/OpenAudioUnity/          # Projeto host Unity + pacote UPM (Packages/com.openaudio.core/)
docs/                          # Especificacoes de formato, referencia de API e tutoriais
```

---

## 2. Regras Inegociaveis de Tempo Real, Concorrencia e Handles

### 2.1. Threading & Real-Time Safety (CRITICO)
1. **Single Game Thread:** A API publica e chamada a partir de uma unica "game thread" (ou message thread na DAW). Nao exige locks entre multiplas game threads, mas **DEVE ser 100% segura em relacao a thread de audio**.
2. **Audio Thread (Callback de Dispositivo / Node Graph do `miniaudio`):**
   - **ESTRITAMENTE PROIBIDO:** Alocar ou liberar memoria na heap (`new`, `delete`, `malloc`, `free`, redimensionar `std::vector` ou `std::string`), bloquear em `std::mutex`, fazer I/O de arquivo, fazer log, lancar excecoes C++ ou invocar callbacks do usuario.
   - **Verificacao em Testes:** Todo teste de renderizacao deve instrumentar um `HeapGuard` (contador atomico nos operadores globais `operator new` / `operator delete` filtrado pelo `thread_id` da thread de renderizacao) e asserir `render_alloc_count == 0`.
3. **Comunicacao Game Thread <-> Audio Thread:**
   - **Comandos discretos:** Filas lock-free Single-Producer Single-Consumer (`SpscQueue<T>`).
   - **Parametros de Strip e Efeitos:** Mailboxes lock-free baseados em *seqlock* (`std::atomic<uint32_t> seq`). O escritor na game thread incrementa `seq` para impar, escreve campos atomicos `relaxed`, e incrementa `seq` para par com `memory_order_release`. O leitor na thread de audio le `seq1` (`acquire`), copia os campos e le `seq2` (`acquire`); se `(seq1 & 1) != 0 || seq1 != seq2`, descarta a leitura e mantem o estado anterior do bloco (nunca faz spin-wait!).
4. **Ordem de Execucao dentro de `oae_system_update(sys)`:**
   1. **Studio `pre_update`:**
      - `1a`: Avanca parametros globais continuos (seek speed) pelo delta de tempo DSP decorrido desde o ultimo `update`.
      - `1b`: Avalia `effect_automations` globais e aplica `oae_effect_set_param` quando o valor mudou.
      - `1c`: Atualiza instancias de eventos ativas: avanca parametros locais -> avalia curvas de automacao -> aplica ganhos/pitch/pan nos canais vivos -> replaneja a timeline se condicoes ou `key_off` mudaram -> agenda instrumentos dentro do horizonte de lookahead (`200 ms`) -> dispara instrumentos `COMMAND`.
      - `1d`: Compoe alvos de Snapshots ativos (ordenados por prioridade) e ganhos de VCAs sobre os buses.
   2. **Callback Dispatch:** Drena a fila pre-alocada de callbacks do Studio e chama a funcao do usuario na game thread. Enquanto um callback esta em execucao, uma flag `in_callback` no sistema faz `oae_system_update`, `oae_system_render`, `oae_system_release`, `oae_studio_unload_bank` e `oae_event_description_load/unload_sample_data` retornarem `OAE_ERR_INVALID_STATE`.
   3. **Core Reclaim:** Destroi canais one-shot ou parados que ja terminaram no relogio DSP, invalidando seus handles no `HandlePool`.
   4. **Voice Virtualization Ranking:** Classifica todos os canais com `is_playing == 1` por `(priority ASC, audibility DESC, creation_order ASC)`. Os primeiros `max_physical_voices` tornam-se **reais**; os demais tornam-se **virtuais**. A audibilidade usa o ganho do canal multiplicado pelos ganhos dos buses ancestrais, com mute valendo zero. Ver [contrato da T05](docs/specs/T05-voice-contract.md).
   5. **Meter Window Swap:** Fecha o acumulador Peak/RMS do periodo concluido em cada bus.

### 2.2. Layout do Handle Geracional (`uint64_t`)
Todo handle publico (`oae_system`, `oae_sound`, `oae_channel`, `oae_bus`, `oae_effect`, `oae_bank`, `oae_event_description`, `oae_event_instance`, `oae_vca`) usa a mesma codificacao de 64 bits:
```cpp
// Bits [0..31]  : index (uint32_t)
// Bits [32..55] : generation (uint32_t, 24 bits, inicia em 1, incrementa no free, se passar de 0xFFFFFF volta para 1)
// Bits [56..63] : type_tag (uint8_t, enum HandleType distinto para cada tipo de objeto)
inline uint64_t make_handle(uint8_t type, uint32_t gen, uint32_t index) noexcept {
    return (static_cast<uint64_t>(type) << 56) |
           (static_cast<uint64_t>(gen & 0x00FFFFFFu) << 32) |
           static_cast<uint64_t>(index);
}
```
- O valor `0` e **sempre invalido** (exceto como alias para o Master Bus em `oae_system_play_sound`, `oae_system_play_sound_at`, `oae_system_play_sound_delayed` e `oae_bus_create`).

### 2.3. Precedencia Universal de Erros na API C
Toda funcao publica valida seus argumentos nesta ordem exata:
1. Ponteiros de saida `NULL` ou argumentos fora de dominio (`std::isnan(x)`, `!std::isfinite(x)`, valor fora da faixa permitida, enum invalido, string vazia/malformada) -> `OAE_ERR_INVALID_PARAM`.
2. Handle invalido, stale, tipo incorreto ou pertencente a outro `oae_system` -> `OAE_ERR_INVALID_HANDLE`.
3. Sistema ainda nao inicializado -> `OAE_ERR_NOT_INITIALIZED`; chamada proibida durante callback ou estado invalido -> `OAE_ERR_INVALID_STATE`.
4. Caminho ou nome nao encontrado -> `OAE_ERR_NOT_FOUND`; caminho duplicado -> `OAE_ERR_ALREADY_EXISTS`.
5. Falha de arquivo/formato -> `OAE_ERR_FILE_NOT_FOUND` / `OAE_ERR_FORMAT` / `OAE_ERR_DEVICE`.
6. Falha de alocacao -> `OAE_ERR_OUT_OF_MEMORY`.
7. Excecao inesperada capturada no `catch (...)` da camada `api/` -> `OAE_ERR_INTERNAL`.

---

## 3. Formulas Matematicas e Algoritmos DSP (Em Sintaxe C++ Pura)

### 3.1. Panning Estereo (Constant-Power Law, Unity at Center)
Para `pan` no intervalo `[-1.0f, +1.0f]`:
```cpp
constexpr float kPi = 3.14159265358979323846f;
constexpr float kSqrt2 = 1.41421356237309504880f;

void compute_pan_gains(float pan, float* out_left, float* out_right) noexcept {
    if (pan == 0.0f) {
        *out_left = 1.0f;
        *out_right = 1.0f;
    } else if (pan <= -1.0f) {
        *out_left = kSqrt2;
        *out_right = 0.0f; // silencio exato no canal oposto
    } else if (pan >= 1.0f) {
        *out_left = 0.0f;  // silencio exato no canal oposto
        *out_right = kSqrt2;
    } else {
        const float theta = (pan + 1.0f) * (kPi * 0.25f);
        *out_left = kSqrt2 * std::cos(theta);
        *out_right = kSqrt2 * std::sin(theta);
    }
}
```

### 3.2. Rampa de De-Click de Pan e Volume de Bus
- Quando `pan` (de canal ou bus) ou `volume` de bus muda apos o primeiro valor inicial, aplica-se uma rampa linear ao longo de `declick_frames`:
```cpp
uint32_t declick_frames(uint32_t sample_rate) noexcept {
    const uint32_t scaled = static_cast<uint32_t>((128ull * sample_rate + 24000ull) / 48000ull);
    return scaled < 16u ? 16u : scaled;
}
```
- O primeiro registro recebido pelo `StripNode` logo apos sua criacao aplica os valores **instantaneamente** (sem rampa).
- Quando a rampa termina (`frames_left == 0`), os ganhos assumem exatamente o valor alvo (`target`), garantindo `0.0f` exato para `volume == 0.0f` ou hard pan.

### 3.3. Curvas de Fade (`FadeSpec`)
Avaliadas na thread de audio como funcao pura do `dsp_clock` (`uint64_t frame_now`):
```cpp
float eval_fade(float from_gain, float to_gain, uint64_t start_frame, uint64_t duration_frames,
                int curve_type, uint64_t frame_now) noexcept {
    if (duration_frames == 0 || frame_now >= start_frame + duration_frames) {
        return to_gain;
    }
    if (frame_now <= start_frame || from_gain == to_gain) {
        return from_gain;
    }
    const float t = static_cast<float>(frame_now - start_frame) / static_cast<float>(duration_frames);
    switch (curve_type) {
        case 0: // OAE_FADE_LINEAR
            return from_gain + (to_gain - from_gain) * t;
        case 1: { // OAE_FADE_LOG (piso de -80 dB = 1e-4f)
            constexpr float kFloor = 1.0e-4f;
            const float a = from_gain < kFloor ? kFloor : from_gain;
            const float b = to_gain < kFloor ? kFloor : to_gain;
            return a * std::pow(b / a, t);
        }
        case 2: { // OAE_FADE_SCURVE (Raised Cosine)
            const float s = 0.5f * (1.0f - std::cos(kPi * t));
            return from_gain + (to_gain - from_gain) * s;
        }
        default:
            return to_gain;
    }
}
```

### 3.4. Envelope Modulador AHDSR (`AhdsrSpec`)
Avaliado na thread de audio como funcao pura do `dsp_clock`:
```cpp
struct AhdsrSpec {
    uint64_t t_start{0};
    uint32_t attack_frames{0};
    uint32_t hold_frames{0};
    uint32_t decay_frames{0};
    uint32_t release_frames{0};
    float sustain{1.0f};
    uint64_t t_rel{~uint64_t{0}}; // ~0ULL = release ainda nao agendado
};

// Valor do envelope antes do release (estagios A -> H -> D -> S)
float ahdsr_pre_release(const AhdsrSpec& s, uint64_t t) noexcept {
    if (t < s.t_start) return 0.0f;
    const uint64_t dt = t - s.t_start;
    if (dt < s.attack_frames) {
        return static_cast<float>(dt) / static_cast<float>(s.attack_frames);
    }
    const uint64_t after_a = dt - s.attack_frames;
    if (after_a < s.hold_frames) {
        return 1.0f;
    }
    const uint64_t after_h = after_a - s.hold_frames;
    if (after_h < s.decay_frames) {
        const float u = static_cast<float>(after_h) / static_cast<float>(s.decay_frames);
        return 1.0f + (s.sustain - 1.0f) * u;
    }
    return s.sustain;
}

float ahdsr_eval(const AhdsrSpec& s, uint64_t t) noexcept {
    if (t < s.t_rel) {
        return ahdsr_pre_release(s, t);
    }
    if (s.release_frames == 0 || t >= s.t_rel + s.release_frames) {
        return 0.0f;
    }
    const float level_at_rel = ahdsr_pre_release(s, s.t_rel);
    const float u = static_cast<float>(t - s.t_rel) / static_cast<float>(s.release_frames);
    return level_at_rel * (1.0f - u);
}
```

### 3.5. Cadeia Multiplicativa do `StripNode` e Recorte de Quinas
No `StripNode` de cada canal, o ganho em um frame `t` e o produto:
```cpp
float gain = channel_vol
           * eval_fade(user_fade, t)
           * eval_fade(instr_fade_in, t)
           * eval_fade(instr_fade_out, t)
           * eval_fade(automation_ramp, t)
           * ahdsr_eval(ahdsr_instr_slot, t)
           * ahdsr_eval(ahdsr_event_slot, t);
```
- **Regra de Subdivisao de Chunk:** O `StripNode` processa blocos de no maximo `256 frames`, mas **recorta o tamanho do sub-bloco** sempre que houver uma quina de envelope dentro do bloco (`start_frame` ou `end_frame` de qualquer fade, fim do de-click, ou qualquer uma das 5 quinas do AHDSR: `t_start`, `t_start + A`, `t_start + A + H`, `t_start + A + H + D`, `t_rel`, `t_rel + R`). Isso garante precisao exata nas quinas.

### 3.6. Suavizacao de Coeficientes de Filtros (`LOWPASS`, `HIGHPASS`, `EQ3`)
Processado em sub-blocos de `32 frames` na thread de audio com constante de tempo `tau = 0.020f` (20 ms):
```cpp
const float k = 1.0f - std::exp(-32.0f / (0.020f * static_cast<float>(sample_rate)));

// Para frequencias (cutoff, low_xover, high_xover) -> interpolacao em dominio logaritmico:
cur_freq = std::exp(std::log(cur_freq) + k * (std::log(target_freq) - std::log(cur_freq)));
if (std::fabs(cur_freq - target_freq) <= 1.0e-4f * target_freq) cur_freq = target_freq;

// Para Q e ganhos em dB do EQ3 -> interpolacao linear na grandeza:
cur_val = cur_val + k * (target_val - cur_val);
if (std::fabs(cur_val - target_val) <= 1.0e-4f) cur_val = target_val;
```

### 3.7. Curvas de Automacao (`curve_eval`)
Para uma curva com pontos `pts[0..N-1]` (`1 <= N <= 32`, `x` estritamente crescente):
```cpp
float curve_eval(const Curve& c, float x) noexcept {
    if (c.points.empty()) return 0.0f;
    if (std::isnan(x) || x <= c.points.front().x) return c.points.front().y;
    if (x >= c.points.back().x) return c.points.back().y;
    for (size_t i = 0; i + 1 < c.points.size(); ++i) {
        const auto& p0 = c.points[i];
        const auto& p1 = c.points[i + 1];
        if (x >= p0.x && x <= p1.x) {
            if (p0.shape == CurveShape::HOLD) return p0.y;
            const float dx = p1.x - p0.x;
            const float t = dx > 0.0f ? (x - p0.x) / dx : 0.0f;
            float u = t;
            switch (p0.shape) {
                case CurveShape::LINEAR:   u = t; break;
                case CurveShape::EASE_IN:  u = t * t; break;
                case CurveShape::EASE_OUT: u = 1.0f - (1.0f - t) * (1.0f - t); break;
                case CurveShape::S_CURVE:  u = t * t * (3.0f - 2.0f * t); break;
                default: break;
            }
            return p0.y + (p1.y - p0.y) * u;
        }
    }
    return c.points.back().y;
}
```

### 3.8. Grid de Batidas (Beat Grid) e Conversao dB / Semitons
```cpp
// Conversao dB -> Ganho Linear (para curvas de automacao, <= -80.0f dB = 0.0f exato):
inline float db_to_linear(float db) noexcept {
    if (db <= -80.0f) return 0.0f; // Silêncio exato para automação e cortes
    return std::pow(10.0f, db / 20.0f);
}

// Conversao Semitons -> Ratio de Pitch:
inline float semitones_to_pitch(float st) noexcept {
    return std::pow(2.0f, st / 12.0f);
}

// Posicao exata em frames da batida n (0-based a partir do marcador de tempo):
// NUNCA acumule somando o periodo; multiplique pelo indice n para evitar drift!
inline uint64_t beat_frame(uint64_t section_start_frame, uint64_t beat_index,
                           double bpm, int32_t ts_lower, uint32_t sample_rate) noexcept {
    const double frames_per_beat = static_cast<double>(sample_rate) * (60.0 / bpm) * (4.0 / static_cast<double>(ts_lower));
    return section_start_frame + static_cast<uint64_t>(std::llround(static_cast<double>(beat_index) * frames_per_beat));
}
```

### 3.9. Geracao Deterministica de GUIDs (FNV-1a 128-bit)
Usado pelo builder interno e pelo compilador `oae_bankc` quando um GUID nao e fornecido explicitamente:
```cpp
uint64_t fnv1a_64(const std::string& s, uint64_t seed) noexcept {
    uint64_t h = seed;
    for (unsigned char c : s) {
        h ^= static_cast<uint64_t>(c);
        h *= 0x100000001b3ull;
    }
    return h;
}

Guid guid_from_string(const std::string& s) noexcept {
    Guid g;
    g.hi = fnv1a_64(s, 0xcbf29ce484222325ull);
    g.lo = fnv1a_64(s, 0x84222325cbf29ce4ull ^ 0x9E3779B97F4A7C15ull);
    if (g.hi == 0 && g.lo == 0) g.lo = 1;
    return g;
}

// Regras de derivacao padrao em assign_default_guids(ModelSet& set):
// - Global Parameter : guid_from_string("global:/" + param.name)
// - Event / Snapshot : guid_from_string(event.path)
// - Local Parameter  : guid_from_string(event.path + "#param:" + param.name)
// - LogicItem[li]    : guid_from_string(event.path + "#logic" + std::to_string(li))
// - Track[ti]        : guid_from_string(event.path + "#track" + std::to_string(ti))
// - Instrument[ti,ii]: guid_from_string(event.path + "#track" + std::to_string(ti) + "#inst" + std::to_string(ii))
```

---

## 4. Especificacao Completa do Formato Binario `.bank` v1 (Byte-a-Byte)

### 4.1. Regras Gerais de Codificacao Wire
- **Endianness:** Todos os inteiros (`u8`, `u16`, `u32`, `i32`, `u64`) e floats (`f32` IEEE-754 binary32, `f64` IEEE-754 binary64) sao **little-endian**. Sem bytes de padding entre campos de registros.
- **Booleans (`bool`):** `u8` onde `0 = false` e `1 = true`. Qualquer byte `> 1` lido num campo booleano deve ser rejeitado imediatamente com `OAE_ERR_FORMAT`.
- **GUID (`Guid`):** 16 bytes compostos por dois `u64` little-endian: **`hi` primeiro, `lo` depois**.
- **Listas (`list<T>`):** Iniciam sempre com `u32 count`, seguido por `count` elementos `T` contiguos. Se `count > remaining_bytes_in_chunk`, rejeitar com `OAE_ERR_FORMAT`.
- **Strings (`str_ref`):** Fora do chunk `STRS`, toda string e gravada como um `u32` (indice zero-based na tabela `STRS`). Ao escrever, as strings sao deduplicadas na ordem em que sao encontradas. Ao ler, se `index >= strings.size()`, rejeitar com `OAE_ERR_FORMAT`.
- **Validacao UTF-8 (`valid_string`):** Nenhuma string na tabela `STRS` pode conter o byte `0x00` (NUL embutido), sequencias UTF-8 invalidas, overlong encodings, surrogates (`0xD800..0xDFFF`) ou codepoints `> 0x10FFFF`.
- **Calculo de CRC-32 (ISO-HDLC / IEEE 802.3):**
```cpp
uint32_t calc_crc32(const uint8_t* data, size_t size) noexcept {
    uint32_t value = 0xFFFFFFFFu;
    while (size--) {
        value ^= *data++;
        for (int bit = 0; bit < 8; ++bit) {
            value = (value >> 1) ^ (0xEDB88320u & (0u - (value & 1u)));
        }
    }
    return ~value;
}
```

### 4.2. Cabecalho do Arquivo e Tabela de Diretorio de Chunks
| Offset | Tipo | Nome | Descricao |
|---|---|---|---|
| `0` | `char[4]` | `magic` | Exatamente 4 bytes ASCII: `'O', 'A', 'E', 'B'` (`0x4245414F` em little-endian). Writer emite somente OAEB; reader rejeita qualquer outro magic com `OAE_ERR_FORMAT` |
| `4` | `u16` | `version` | `1` (qualquer valor `!= 1` retorna `OAE_ERR_FORMAT`) |
| `6` | `u16` | `flags` | `0` (qualquer valor `!= 0` retorna `OAE_ERR_FORMAT`) |
| `8` | `u32` | `chunk_count` | Quantidade de registros no diretorio |
| `12` | `DirEntry[chunk_count]` | `directory` | `24 bytes` por entrada (ver abaixo) |

**Estrutura de cada `DirEntry` (24 bytes):**
- `tag`: `char[4]` (ex: `"STRS"`, `"BUSS"`, `"EVTS"`, `"SMPL"`, `"DATA"`, `"LOGC"`, `"PARM"`, `"SNAP"`, `"VCAS"`).
- `offset`: `u64` (offset absoluto desde o byte 0 do arquivo; **deve ser multiplo de 16** e `>= 12 + chunk_count * 24`).
- `length`: `u64` (tamanho em bytes do payload do chunk).
- `crc32`: `u32` (`calc_crc32` sobre os `length` bytes do payload).

**Regras de Validacao do Diretorio:**
1. Nao pode haver duas entradas com a mesma `tag` (`u32`).
2. Nenhum payload pode ultrapassar o fim do arquivo (`offset > file_size || length > file_size - offset`) nem se sobrepor a outro chunk com `length > 0`.
3. O `crc32` de todos os chunks presentes (inclusive chunks de tags desconhecidas) e verificado antes de qualquer parse; se falhar -> `OAE_ERR_FORMAT`.
4. Os 5 chunks core (`STRS`, `BUSS`, `EVTS`, `SMPL`, `DATA`) sao **obrigatorios**. Os 4 chunks de extensao (`LOGC`, `PARM`, `SNAP`, `VCAS`) sao opcionais na leitura (bancos minimos sem eles mantem os campos em default) e sempre emitidos pelo writer completo.
5. Para todos os chunks conhecidos exceto `DATA`, ao terminar de ler o chunk, `remaining_bytes` deve ser `0` (bytes extras no final do chunk retornam `OAE_ERR_FORMAT`).

### 4.3. Layout Exato dos 9 Chunks

#### Chunk 0: `STRS` (String Table)
- `u32 string_count`
- Para cada string (`0 .. string_count - 1`):
  - `u32 byte_length`
  - `u8[byte_length]` (bytes UTF-8 validos, sem terminador NUL)

#### Chunk 1: `BUSS` (Mixer Buses & Effect Chains)
- `u32 bus_count`
- Para cada bus (`0 .. bus_count - 1`):
  - `u32 name_str` (indice em `STRS`; nao pode ser vazio)
  - `i32 parent_bus_index` (`-1` para Master Bus; caso contrario `0 <= parent_bus_index < current_bus_index` — garante ausência de ciclos por construcao)
  - `f32 volume` (ganho linear finito `>= 0.0f`)
  - `u32 effect_count`
  - Para cada efeito (`0 .. effect_count - 1`):
    - `i32 position` (`0 = POST_FADER`, `1 = PRE_FADER`; max 8 efeitos por posicao por bus)
    - `i32 type` (`0=LIMITER, 1=COMPRESSOR, 2=EQ3, 3=LOWPASS, 4=HIGHPASS, 5=REVERB, 6=DELAY`)
    - `bool bypass` (`u8`: `0` ou `1`)
    - `i32 sidechain_bus_index` (`-1` para nenhum; ou indice valido em `BUSS` diferente do proprio bus, permitido apenas quando `type == 1 (COMPRESSOR)`)
    - `u32 param_count` (deve ser exatamente o numero de parametros nativos do `type`: `LIMITER=3`, `COMPRESSOR=5`, `EQ3=5`, `LOWPASS=2`, `HIGHPASS=2`, `REVERB=3`, `DELAY=2`)
    - `f32 params[param_count]` (cada valor finito e dentro da faixa nativa do parametro)
    - `f32 wet` (finito em `[0.0f, 1.0f]`)
    - `f32 dry` (finito em `[0.0f, 1.0f]`)

#### Chunk 2: `EVTS` (Events, Tracks & Instruments)
- `u32 event_count`
- Para cada evento (`0 .. event_count - 1`):
  - `u64 id.hi`, `u64 id.lo` (`Guid` nao-nulo e unico)
  - `u32 path_str` (indice em `STRS`; deve comecar com `"event:/"` ou `"snapshot:/"`)
  - `u32 bus_path_str` (indice em `STRS`; `""` ou `"bus:/..."`)
  - `f32 volume_db` (`|v| <= 200.0f`)
  - `f32 pitch_semitones` (`|v| <= 200.0f`)
  - `f32 pan` (`[-1.0f, +1.0f]`)
  - `u32 max_instances` (`0` = ilimitado)
  - `u8 steal` (`0 = OLDEST`, `1 = QUIETEST`, `2 = NONE`)
  - `u32 cooldown_ms` (`<= 3600000`)
  - `i32 priority` (`0 .. 256`)
  - `u32 stop_fadeout_ms` (`<= 3600000`)
  - `u32 track_count`
  - Para cada track (`0 .. track_count - 1`):
    - `u64 id.hi`, `u64 id.lo`
    - `f32 volume_db` (`|v| <= 200.0f`)
    - `f32 pan` (`[-1.0f, +1.0f]`)
    - `u32 instrument_count`
    - Para cada instrumento (`0 .. instrument_count - 1`) — **todos os campos abaixo estao sempre presentes**:
      - `u64 id.hi`, `u64 id.lo`
      - `u8 kind` (`0 = SINGLE`, `1 = MULTI`, `2 = EVENT`, `3 = COMMAND`, `4 = SILENCE`)
      - `f64 offset_ms` (`0.0 .. 86400000.0`)
      - `f64 length_ms` (`0.0 .. 86400000.0`)
      - `f64 start_offset_ms` (`0.0 .. 86400000.0`)
      - `bool loop` (`u8`: `0` ou `1`)
      - `f64 fade_in_ms` (`0.0 .. 86400000.0`)
      - `f64 fade_out_ms` (`0.0 .. 86400000.0`)
      - `f32 volume_db.base`, `f32 volume_db.random_range`
      - `f32 pitch_semitones.base`, `f32 pitch_semitones.random_range`
      - `i32 sample_index` (indice em `SMPL` quando `kind == SINGLE`, ou `-1`)
      - `u64 event_ref.hi`, `u64 event_ref.lo` (`Guid` do evento filho quando `kind == EVENT`)
      - `u8 command.type` (`0 = START_EVENT`, `1 = STOP_EVENT`, `2 = SNAPSHOT_ON`, `3 = SNAPSHOT_OFF`)
      - `u64 command.target.hi`, `u64 command.target.lo` (`Guid` alvo quando `kind == COMMAND`)
      - `u8 playlist.mode` (`0 = RANDOM`, `1 = RANDOM_NO_REPEAT`, `2 = SEQUENTIAL`, `3 = SHUFFLE`)
      - `u8 playlist.scope` (`0 = GLOBAL`, `1 = INSTANCE`)
      - `u32 playlist_entry_count`
      - Para cada entrada de playlist (`0 .. playlist_entry_count - 1`):
        - `i32 sample_index` (`>= 0` se sample, `-1` se evento)
        - `u64 event.hi`, `u64 event.lo` (nao-nulo se evento, `{0, 0}` se sample)
        - `f32 weight` (`> 0.0f` e finito)
  - `u32 user_prop_count`
  - Para cada user_prop (`0 .. user_prop_count - 1`):
    - `u32 key_str` (indice em `STRS`)
    - `u32 value_str` (indice em `STRS`)

#### Chunk 3: `SMPL` (Sample Index)
- `u32 sample_count`
- Para cada sample (`0 .. sample_count - 1`):
  - `u64 data_offset` (offset relativo ao inicio do chunk `DATA`, **multiplo de 16**)
  - `u64 byte_length` (`> 0`, dentro dos limites de `DATA`, intervalos disjuntos nao-sobrepostos)
  - `u64 length_frames` (`> 0`, total de frames PCM decodificados)
  - `u32 sample_rate` (`> 0`, taxa de amostragem em Hz)

#### Chunk 4: `DATA` (Encoded Audio Blobs)
- Bytes brutos dos arquivos de audio originais (WAV, OGG, MP3, FLAC), alinhados em multiplos de 16 bytes com zero-padding entre um sample e outro.

#### Chunk 5: `LOGC` (Timeline Logic Items)
- `u32 event_count` (**deve ser igual ao `event_count` de `EVTS`**)
- Para cada evento `i` (`0 .. event_count - 1`, na exata mesma ordem de `EVTS`):
  - `u64 event_id.hi`, `u64 event_id.lo` (**deve ser igual a `EVTS[i].id`**)
  - `u32 logic_count`
  - Para cada `LogicItem` (`0 .. logic_count - 1`):
    - `u8 kind` (`0=MARKER, 1=TEMPO, 2=LOOP_REGION, 3=SUSTAIN_POINT, 4=TRANSITION_MARKER, 5=TRANSITION_REGION`)
    - `u64 id.hi`, `u64 id.lo`
    - `f64 position_ms`
    - `f64 end_ms`
    - `u32 name_str` (indice em `STRS`)
    - `f64 bpm`
    - `i32 ts_upper`
    - `i32 ts_lower`
    - `i32 repeat_count`
    - `bool until_key_off` (`u8`)
    - `u32 destination_str` (indice em `STRS`)
    - `u8 quantization` (`0=IMMEDIATE, 1=BEAT, 2=BAR`)
    - `f64 crossfade_ms`
    - `i32 condition_index` (`-1 .. 63`)

#### Chunk 6: `PARM` (Global Parameters, Per-Event Parameters/Conditions/Modulators/Automations & Effect Automations)
> **Sub-rotina `ParameterRecord`:**
> `u64 id.hi`, `u64 id.lo`, `u32 name_str`, `u8 kind` (`0=CONTINUOUS, 1=DISCRETE, 2=LABELED`), `f32 min`, `f32 max`, `f32 default_value`, `f32 seek_speed`, `f32 seek_speed_down`, `u32 label_count`, seguido de `u32 label_str[label_count]`.
>
> **Sub-rotina `ParamRefRecord`:**
> `bool global` (`u8`), `u32 local_index`, `u64 global_id.hi`, `u64 global_id.lo`.
>
> **Sub-rotina `CurveRecord`:**
> `u32 point_count`, seguido de `point_count` registros: `f32 x`, `f32 y`, `u8 shape` (`0=LINEAR, 1=HOLD, 2=EASE_IN, 3=EASE_OUT, 4=S_CURVE`).

**Corpo do Chunk `PARM`:**
1. **Global Parameters:** `u32 global_param_count` + `ParameterRecord[global_param_count]`.
2. **Per-Event Records:** `u32 event_count` (igual a `EVTS.size()`), e para cada evento `i`:
   - `u64 event_id.hi`, `u64 event_id.lo` (igual a `EVTS[i].id`)
   - `u32 local_param_count` + `ParameterRecord[local_param_count]`
   - `u32 condition_count`, e para cada `Condition`:
     - `f32 probability_pct`
     - `u32 term_count`, e para cada `ConditionTerm`: `ParamRefRecord param`, `f32 min`, `f32 max`
   - `u32 modulator_count`, e para cada `Modulator`:
     - `u8 kind` (`0=RANDOM, 1=AHDSR`), `u8 owner` (`0=EVENT, 1=TRACK, 2=INSTRUMENT`), `u32 owner_index`, `u8 prop` (`0=VOLUME_DB, 1=PITCH_ST, 2=PAN`), `f32 range`, `f32 attack_ms`, `f32 hold_ms`, `f32 decay_ms`, `f32 sustain`, `f32 release_ms`, `bool release_on_key_off` (`u8`)
   - `u32 automation_count`, e para cada `Automation`:
     - `u8 owner`, `u32 owner_index`, `u8 prop`, `u8 source` (`0=PARAMETER, 1=TIMELINE`), `ParamRefRecord param`, `CurveRecord curve`
   - `u32 flat_instrument_count` (**deve ser igual a soma de `tracks[t].instruments.size()` do evento `i`**)
   - `i32 condition_index[flat_instrument_count]` (um `i32` para cada instrumento do evento, na ordem achatada track por track)
3. **Effect Automations:** `u32 effect_auto_count`, e para cada `EffectAutomation`:
   - `u32 bus_path_str`, `i32 position`, `u32 index`, `i32 effect_type`, `i32 param_id`, `u64 global_param.hi`, `u64 global_param.lo`, `CurveRecord curve`.

#### Chunk 7: `SNAP` (Snapshot Metadata & Targets)
- `u32 event_count` (igual a `EVTS.size()`)
- Para cada evento `i` (`0 .. event_count - 1`):
  - `u64 event_id.hi`, `u64 event_id.lo` (igual a `EVTS[i].id`)
  - `u8 event_kind` (`0 = EVENT`, `1 = SNAPSHOT`)
  - `u8 snapshot_mode` (`0 = OVERRIDE`, `1 = BLENDING`)
  - `u32 fade_in_ms`
  - `i32 intensity_param` (`-1` para nenhum, ou indice de parametro local `CONTINUOUS` com range `0..100`)
  - `u32 target_count`
  - Para cada `SnapshotTarget` (`0 .. target_count - 1`):
    - `u32 bus_path_str` (indice em `STRS`)
    - `f32 value` (dB relativo se `param_id == -1`, ou valor em unidades nativas do efeito)
    - `i32 param_id` (`-1` = volume do bus; `>= 0` = ID do parametro de efeito)
    - `i32 position` (`0 = POST_FADER`, `1 = PRE_FADER`)
    - `u32 index` (indice do efeito na cadeia daquela posicao)
    - `i32 effect_type` (`oae_effect_type`)

#### Chunk 8: `VCAS` (Voltage Controlled Amplifiers)
- `u32 vca_count`
- Para cada VCA (`0 .. vca_count - 1`):
  - `u32 path_str` (indice em `STRS`, deve comecar com `"vca:/"`)
  - `u32 bus_count` + `u32 bus_path_str[bus_count]`
  - `u32 event_count` + `u32 event_path_str[event_count]`

---

### 4.4. Schema JSON de Projeto (`project.json` v1) e Catálogo (`.bank.catalog.json`)
- Raiz do `project.json`:
  ```json
  {
    "version": 1,
    "samples": [{ "id": "footstep", "file": "assets/footstep.wav" }],
    "buses": [{ "path": "bus:/sfx", "volume": 1.0, "effects": [] }],
    "global_parameters": [{ "name": "RPM", "kind": "continuous", "min": 0, "max": 8000, "default_value": 0, "seek_speed": 4000 }],
    "events": [],
    "effect_automations": [],
    "vcas": [{ "path": "vca:/sfx", "buses": ["bus:/sfx"], "events": [] }],
    "mixer": { "master": { "volume": 1.0, "mute": false } }
  }
  ```
- O catálogo `.bank.catalog.json` contém `{ "version": 1, "bank_size": <bytes>, "bank_crc32": <u32>, "events": [{ "path": "event:/...", "kind": "event" }] }`.
- `bank_size` e `bank_crc32` descrevem o banco publicado correspondente; consumidores verificam a correspondência antes de usar o catálogo.
- O compilador resolve os caminhos de áudio relativamente ao JSON. Referências a arquivos, buses, markers, parâmetros, labels ou eventos inexistentes e ciclos de eventos aninhados devem gerar diagnóstico com JSON path e código de saída diferente de zero.
- O Studio importa áudio para a pasta do projeto com caminhos relativos, preservando portabilidade, e permite relink de arquivos ausentes.

A estrutura, os defaults, os objetos aninhados e a validação semântica estão no [contrato de autoria v1](docs/specs/project-json-v1-proposal.md), aprovado na [decisão 001](docs/decisions/001-contratos-autoria-e-publicacao-v1.md).

### 4.5. Transação de Publicação do `oae_bankc`

1. Gerar e validar `.bank` e `.bank.catalog.json` em uma pasta temporária `.bankc-stage` ao lado do destino.
2. Preservar o catálogo anterior para permitir rollback.
3. Substituir atomicamente o catálogo `.bank.catalog.json` primeiro.
4. Substituir atomicamente o `.bank` depois.
5. Se a substituição do `.bank` falhar, restaurar o catálogo antigo e encerrar com código diferente de zero. Se não havia catálogo anterior, remover o catálogo recém-publicado no rollback.
6. Só reportar sucesso após publicar ambos. Falhas de geração ou validação não devem modificar o par anterior.

A atomicidade é de cada substituição: a sequência não equivale a uma troca indivisível dos dois arquivos. Durante o intervalo entre as substituições, os consumidores devem detectar incompatibilidade usando tamanho/CRC do banco. O [contrato de publicação e recuperação](docs/specs/publication-recovery-proposal.md) define concorrência, interrupção de processo e falha do rollback.

---

## 5. Superficie Completa da API C (`native/include/oae_audio.h`)

Abaixo estao todas as estruturas blittable, constantes, enums e as 100 funcoes da ABI C publica:

```c
#ifndef OAE_AUDIO_H
#define OAE_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(OAE_AUDIO_BUILD)
#    define OAE_API __declspec(dllexport)
#  else
#    define OAE_API __declspec(dllimport)
#  endif
#  define OAE_CALL __cdecl
#else
#  define OAE_API __attribute__((visibility("default")))
#  define OAE_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum oae_result {
    OAE_OK = 0,
    OAE_ERR_INVALID_PARAM = 1,
    OAE_ERR_INVALID_HANDLE = 2,
    OAE_ERR_NOT_INITIALIZED = 3,
    OAE_ERR_ALREADY_INITIALIZED = 4,
    OAE_ERR_OUT_OF_MEMORY = 5,
    OAE_ERR_FILE_NOT_FOUND = 6,
    OAE_ERR_FORMAT = 7,
    OAE_ERR_DEVICE = 8,
    OAE_ERR_NOT_FOUND = 9,
    OAE_ERR_ALREADY_EXISTS = 10,
    OAE_ERR_INVALID_STATE = 11,
    OAE_ERR_INTERNAL = 100
} oae_result;

typedef uint64_t oae_system;
typedef uint64_t oae_sound;
typedef uint64_t oae_channel;
typedef uint64_t oae_bus;
typedef uint64_t oae_effect;
typedef uint64_t oae_bank;
typedef uint64_t oae_event_description;
typedef uint64_t oae_event_instance;
typedef uint64_t oae_vca;

typedef enum oae_init_flags {
    OAE_INIT_DEFAULT   = 0,
    OAE_INIT_NO_DEVICE = (1 << 0)
} oae_init_flags;

typedef struct oae_init_settings {
    uint32_t sample_rate;         /* default: 48000 */
    uint32_t channels;            /* default: 2 */
    uint32_t max_physical_voices; /* default: 32, must be > 0 */
    uint32_t flags;               /* bitmask of oae_init_flags */
} oae_init_settings;

typedef enum oae_sound_flags {
    OAE_SOUND_DEFAULT = 0,
    OAE_SOUND_LOOP    = (1 << 0),
    OAE_SOUND_STREAM  = (1 << 1)
} oae_sound_flags;

typedef enum oae_fade_curve {
    OAE_FADE_LINEAR = 0,
    OAE_FADE_LOG    = 1,
    OAE_FADE_SCURVE = 2
} oae_fade_curve;

typedef enum oae_effect_type {
    OAE_EFFECT_LIMITER    = 0,
    OAE_EFFECT_COMPRESSOR = 1,
    OAE_EFFECT_EQ3        = 2,
    OAE_EFFECT_LOWPASS    = 3,
    OAE_EFFECT_HIGHPASS   = 4,
    OAE_EFFECT_REVERB     = 5,
    OAE_EFFECT_DELAY      = 6
} oae_effect_type;

typedef enum oae_effect_position {
    OAE_EFFECT_POST_FADER = 0,
    OAE_EFFECT_PRE_FADER  = 1
} oae_effect_position;

#define OAE_EFFECT_INDEX_LAST (-1)

typedef enum oae_limiter_param {
    OAE_LIMITER_CEILING_DB    = 0, /* -12..0, default -0.1 */
    OAE_LIMITER_RELEASE_MS    = 1, /* 1..1000, default 50 */
    OAE_LIMITER_INPUT_GAIN_DB = 2  /* 0..24, default 0 */
} oae_limiter_param;

typedef enum oae_compressor_param {
    OAE_COMPRESSOR_THRESHOLD_DB = 0, /* -60..0, default -20 */
    OAE_COMPRESSOR_RATIO        = 1, /* 1..20, default 4 */
    OAE_COMPRESSOR_ATTACK_MS    = 2, /* 0.1..500, default 10 */
    OAE_COMPRESSOR_RELEASE_MS   = 3, /* 1..5000, default 100 */
    OAE_COMPRESSOR_MAKEUP_DB    = 4  /* 0..24, default 0 */
} oae_compressor_param;

typedef enum oae_eq3_param {
    OAE_EQ3_LOW_DB        = 0, /* -80..12, default 0 */
    OAE_EQ3_MID_DB        = 1, /* -80..12, default 0 */
    OAE_EQ3_HIGH_DB       = 2, /* -80..12, default 0 */
    OAE_EQ3_LOW_XOVER_HZ  = 3, /* 20..20000, default 400 */
    OAE_EQ3_HIGH_XOVER_HZ = 4  /* 20..20000, default 4000 */
} oae_eq3_param;

typedef enum oae_filter_param {
    OAE_FILTER_CUTOFF_HZ = 0, /* 10..20000, default 20000 (LPF) / 10 (HPF) */
    OAE_FILTER_Q         = 1  /* 0.1..10, default 0.7071 */
} oae_filter_param;

typedef enum oae_reverb_param {
    OAE_REVERB_ROOM_SIZE = 0, /* 0..1, default 0.5 */
    OAE_REVERB_DAMPING   = 1, /* 0..1, default 0.5 */
    OAE_REVERB_WIDTH     = 2  /* 0..1, default 1.0 */
} oae_reverb_param;

typedef enum oae_delay_param {
    OAE_DELAY_TIME_MS  = 0, /* 1..2000, default 300 */
    OAE_DELAY_FEEDBACK = 1  /* 0..0.95, default 0.3 */
} oae_delay_param;

#define OAE_METER_MAX_CHANNELS 8
typedef struct oae_meter {
    uint32_t channel_count;
    uint32_t frame_count;
    float peak[OAE_METER_MAX_CHANNELS];
    float rms[OAE_METER_MAX_CHANNELS];
} oae_meter;

typedef enum oae_playback_state {
    OAE_PLAYBACK_PLAYING    = 0,
    OAE_PLAYBACK_SUSTAINING = 1,
    OAE_PLAYBACK_STOPPED    = 2,
    OAE_PLAYBACK_STARTING   = 3,
    OAE_PLAYBACK_STOPPING   = 4
} oae_playback_state;

typedef enum oae_stop_mode {
    OAE_STOP_ALLOW_FADEOUT = 0,
    OAE_STOP_IMMEDIATE     = 1
} oae_stop_mode;

typedef enum oae_event_callback_type {
    OAE_EVENT_CALLBACK_STARTED       = 1 << 0,
    OAE_EVENT_CALLBACK_STOPPED       = 1 << 1,
    OAE_EVENT_CALLBACK_MARKER        = 1 << 2,
    OAE_EVENT_CALLBACK_BEAT          = 1 << 3,
    OAE_EVENT_CALLBACK_STOLEN        = 1 << 4,
    OAE_EVENT_CALLBACK_SOUND_PLAYED  = 1 << 5,
    OAE_EVENT_CALLBACK_SOUND_STOPPED = 1 << 6,
    OAE_EVENT_CALLBACK_ALL           = 0x7F
} oae_event_callback_type;

typedef struct oae_timeline_marker_properties {
    uint64_t dsp_clock;
    const char* name;
    int32_t position_ms;
    uint32_t marker_index;
} oae_timeline_marker_properties;

typedef struct oae_timeline_beat_properties {
    uint64_t dsp_clock;
    int32_t bar;
    int32_t beat;
    int32_t position_ms;
    float tempo;
    int32_t time_signature_upper;
    int32_t time_signature_lower;
} oae_timeline_beat_properties;

typedef struct oae_event_sound_properties {
    uint64_t dsp_clock;
    oae_sound sound;
    oae_channel channel;
    uint32_t instrument_index;
    uint32_t nested_depth;
} oae_event_sound_properties;

typedef oae_result (OAE_CALL *oae_event_callback)(
    uint32_t type, oae_event_instance instance, const void* properties, void* user_data);

typedef struct oae_struct_sizes {
    uint32_t init_settings;
    uint32_t meter;
    uint32_t timeline_marker_properties;
    uint32_t timeline_beat_properties;
    uint32_t event_sound_properties;
} oae_struct_sizes;

enum { OAE_LOAD_BANK_NONBLOCKING = 1u << 0 };
typedef enum oae_bank_loading_state {
    OAE_BANK_LOADING = 0,
    OAE_BANK_LOADED  = 1,
    OAE_BANK_FAILED  = 2
} oae_bank_loading_state;

/* Funcoes Exportadas (100 funcoes): */
OAE_API uint32_t OAE_CALL oae_get_version(void);
OAE_API const char* OAE_CALL oae_result_string(oae_result result);
OAE_API oae_result OAE_CALL oae_init_settings_default(oae_init_settings* settings);
OAE_API oae_result OAE_CALL oae_system_create(oae_system* out_system);
OAE_API oae_result OAE_CALL oae_system_init(oae_system system, const oae_init_settings* settings);
OAE_API oae_result OAE_CALL oae_system_update(oae_system system);
OAE_API oae_result OAE_CALL oae_system_render(oae_system system, float* out, uint32_t frames);
OAE_API oae_result OAE_CALL oae_system_release(oae_system system);

OAE_API oae_result OAE_CALL oae_sound_create_from_file(oae_system system, const char* path, uint32_t flags, oae_sound* out_sound);
OAE_API oae_result OAE_CALL oae_sound_create_from_memory(oae_system system, const void* data, size_t size, uint32_t flags, oae_sound* out_sound);
OAE_API oae_result OAE_CALL oae_sound_release(oae_sound sound);
OAE_API oae_result OAE_CALL oae_sound_get_length_ms(oae_sound sound, uint32_t* out_ms);

OAE_API oae_result OAE_CALL oae_system_play_sound(oae_system system, oae_sound sound, oae_bus bus, int paused, oae_channel* out_channel);
OAE_API oae_result OAE_CALL oae_channel_stop(oae_channel channel);
OAE_API oae_result OAE_CALL oae_channel_set_paused(oae_channel channel, int paused);
OAE_API oae_result OAE_CALL oae_channel_get_paused(oae_channel channel, int* out_paused);
OAE_API oae_result OAE_CALL oae_channel_set_volume(oae_channel channel, float volume);
OAE_API oae_result OAE_CALL oae_channel_get_volume(oae_channel channel, float* out_volume);
OAE_API oae_result OAE_CALL oae_channel_set_pitch(oae_channel channel, float pitch);
OAE_API oae_result OAE_CALL oae_channel_get_pitch(oae_channel channel, float* out_pitch);
OAE_API oae_result OAE_CALL oae_channel_is_playing(oae_channel channel, int* out_is_playing);
OAE_API oae_result OAE_CALL oae_channel_get_position_ms(oae_channel channel, uint32_t* out_ms);
OAE_API oae_result OAE_CALL oae_channel_set_position_ms(oae_channel channel, uint32_t ms);

OAE_API oae_result OAE_CALL oae_system_get_master_bus(oae_system system, oae_bus* out_bus);
OAE_API oae_result OAE_CALL oae_bus_create(oae_system system, const char* name, oae_bus parent, oae_bus* out_bus);
OAE_API oae_result OAE_CALL oae_system_get_bus(oae_system system, const char* path, oae_bus* out_bus);
OAE_API oae_result OAE_CALL oae_bus_set_volume(oae_bus bus, float volume);
OAE_API oae_result OAE_CALL oae_bus_get_volume(oae_bus bus, float* out_volume);
OAE_API oae_result OAE_CALL oae_bus_set_mute(oae_bus bus, int mute);
OAE_API oae_result OAE_CALL oae_bus_get_mute(oae_bus bus, int* out_mute);
OAE_API oae_result OAE_CALL oae_bus_set_paused(oae_bus bus, int paused);
OAE_API oae_result OAE_CALL oae_bus_get_paused(oae_bus bus, int* out_paused);
OAE_API oae_result OAE_CALL oae_bus_stop_all(oae_bus bus);
OAE_API oae_result OAE_CALL oae_bus_release(oae_bus bus);

OAE_API oae_result OAE_CALL oae_channel_set_priority(oae_channel channel, int priority);
OAE_API oae_result OAE_CALL oae_channel_get_priority(oae_channel channel, int* out_priority);
OAE_API oae_result OAE_CALL oae_channel_is_virtual(oae_channel channel, int* out_virtual);
OAE_API oae_result OAE_CALL oae_system_get_channels_playing(oae_system system, int* total, int* real);

OAE_API oae_result OAE_CALL oae_channel_set_pan(oae_channel channel, float pan);
OAE_API oae_result OAE_CALL oae_channel_get_pan(oae_channel channel, float* out_pan);
OAE_API oae_result OAE_CALL oae_bus_set_pan(oae_bus bus, float pan);
OAE_API oae_result OAE_CALL oae_bus_get_pan(oae_bus bus, float* out_pan);
OAE_API oae_result OAE_CALL oae_channel_fade_to(oae_channel channel, float target, uint32_t duration_ms, int curve);
OAE_API oae_result OAE_CALL oae_bus_fade_to(oae_bus bus, float target, uint32_t duration_ms, int curve);
OAE_API oae_result OAE_CALL oae_channel_get_fade_gain(oae_channel channel, float* out_gain);
OAE_API oae_result OAE_CALL oae_bus_get_fade_gain(oae_bus bus, float* out_gain);
OAE_API oae_result OAE_CALL oae_channel_stop_with_fade(oae_channel channel, uint32_t duration_ms);
OAE_API oae_result OAE_CALL oae_system_get_dsp_clock(oae_system system, uint64_t* out_frames);
OAE_API oae_result OAE_CALL oae_system_play_sound_at(oae_system system, oae_sound sound, oae_bus bus, uint64_t dsp_clock, oae_channel* out_channel);
OAE_API oae_result OAE_CALL oae_system_play_sound_delayed(oae_system system, oae_sound sound, oae_bus bus, uint32_t delay_ms, oae_channel* out_channel);

OAE_API oae_result OAE_CALL oae_bus_add_effect(oae_bus bus, int type, int32_t index, oae_effect* out_effect);
OAE_API oae_result OAE_CALL oae_bus_add_effect_ex(oae_bus bus, int type, int position, int32_t index, oae_effect* out_effect);
OAE_API oae_result OAE_CALL oae_effect_set_param(oae_effect effect, int32_t param_id, float value);
OAE_API oae_result OAE_CALL oae_effect_get_param(oae_effect effect, int32_t param_id, float* out_value);
OAE_API oae_result OAE_CALL oae_effect_set_bypass(oae_effect effect, int bypass);
OAE_API oae_result OAE_CALL oae_effect_get_bypass(oae_effect effect, int* out_bypass);
OAE_API oae_result OAE_CALL oae_effect_set_wet_dry(oae_effect effect, float wet, float dry);
OAE_API oae_result OAE_CALL oae_effect_get_wet_dry(oae_effect effect, float* out_wet, float* out_dry);
OAE_API oae_result OAE_CALL oae_effect_set_sidechain(oae_effect effect, oae_bus source_bus);
OAE_API oae_result OAE_CALL oae_effect_release(oae_effect effect);
OAE_API oae_result OAE_CALL oae_bus_get_meter(oae_bus bus, oae_meter* out_meter);

OAE_API oae_result OAE_CALL oae_studio_load_bank_memory(oae_system system, const void* bytes, size_t size, oae_bank* out_bank);
OAE_API oae_result OAE_CALL oae_studio_load_bank_file(oae_system system, const char* path, oae_bank* out_bank);
OAE_API oae_result OAE_CALL oae_studio_unload_bank(oae_system system, oae_bank bank);
OAE_API oae_result OAE_CALL oae_studio_get_vca(oae_system system, const char* path, oae_vca* out_vca);
OAE_API oae_result OAE_CALL oae_vca_set_volume(oae_vca vca, float volume);
OAE_API oae_result OAE_CALL oae_vca_get_volume(oae_vca vca, float* out_volume);
OAE_API oae_result OAE_CALL oae_studio_get_event(oae_system system, const char* path, oae_event_description* out_description);
OAE_API oae_result OAE_CALL oae_studio_play_one_shot(oae_system system, const char* path);
OAE_API oae_result OAE_CALL oae_event_description_create_instance(oae_event_description description, oae_event_instance* out_instance);
OAE_API oae_result OAE_CALL oae_event_description_get_length_ms(oae_event_description description, uint32_t* out_length_ms);
OAE_API oae_result OAE_CALL oae_event_description_is_oneshot(oae_event_description description, int* out_is_oneshot);
OAE_API oae_result OAE_CALL oae_event_description_get_instance_count(oae_event_description description, int* out_count);
OAE_API oae_result OAE_CALL oae_event_instance_start(oae_event_instance instance);
OAE_API oae_result OAE_CALL oae_event_instance_stop(oae_event_instance instance, int mode);
OAE_API oae_result OAE_CALL oae_event_instance_release(oae_event_instance instance);
OAE_API oae_result OAE_CALL oae_event_instance_get_playback_state(oae_event_instance instance, int* out_state);
OAE_API oae_result OAE_CALL oae_event_instance_get_description(oae_event_instance instance, oae_event_description* out_description);
OAE_API oae_result OAE_CALL oae_event_instance_set_volume(oae_event_instance instance, float volume);
OAE_API oae_result OAE_CALL oae_event_instance_get_volume(oae_event_instance instance, float* out_volume);
OAE_API oae_result OAE_CALL oae_event_instance_set_pitch(oae_event_instance instance, float pitch);
OAE_API oae_result OAE_CALL oae_event_instance_get_pitch(oae_event_instance instance, float* out_pitch);
OAE_API oae_result OAE_CALL oae_event_instance_set_pan(oae_event_instance instance, float pan);
OAE_API oae_result OAE_CALL oae_event_instance_get_pan(oae_event_instance instance, float* out_pan);

OAE_API oae_result OAE_CALL oae_event_instance_set_callback(oae_event_instance instance, oae_event_callback callback, uint32_t mask, void* user_data);
OAE_API oae_result OAE_CALL oae_event_instance_key_off(oae_event_instance instance);
OAE_API oae_result OAE_CALL oae_event_instance_get_timeline_position_ms(oae_event_instance instance, int32_t* out_position_ms);
OAE_API oae_result OAE_CALL oae_event_instance_set_timeline_position_ms(oae_event_instance instance, int32_t position_ms);

OAE_API oae_result OAE_CALL oae_event_instance_set_parameter_by_name(oae_event_instance instance, const char* name, float value, int ignore_seek_speed);
OAE_API oae_result OAE_CALL oae_event_instance_set_parameter_by_name_with_label(oae_event_instance instance, const char* name, const char* label, int ignore_seek_speed);
OAE_API oae_result OAE_CALL oae_event_instance_get_parameter_by_name(oae_event_instance instance, const char* name, float* out_value, float* out_final_value);
OAE_API oae_result OAE_CALL oae_studio_set_global_parameter_by_name(oae_system system, const char* name, float value, int ignore_seek_speed);
OAE_API oae_result OAE_CALL oae_studio_set_global_parameter_by_name_with_label(oae_system system, const char* name, const char* label, int ignore_seek_speed);
OAE_API oae_result OAE_CALL oae_studio_get_global_parameter_by_name(oae_system system, const char* name, float* out_value, float* out_final_value);

OAE_API oae_result OAE_CALL oae_get_struct_sizes(oae_struct_sizes* out_sizes);
OAE_API oae_result OAE_CALL oae_studio_load_bank_file_ex(oae_system system, const char* path, uint32_t flags, oae_bank* out_bank);
OAE_API oae_result OAE_CALL oae_studio_load_bank_memory_ex(oae_system system, const void* bytes, size_t size, uint32_t flags, oae_bank* out_bank);
OAE_API oae_result OAE_CALL oae_studio_get_bank_loading_state(oae_system system, oae_bank bank, int* out_state);
OAE_API oae_result OAE_CALL oae_event_description_load_sample_data(oae_event_description description);
OAE_API oae_result OAE_CALL oae_event_description_unload_sample_data(oae_event_description description);

#ifdef __cplusplus
}
#endif
#endif /* OAE_AUDIO_H */
```

---

## 6. O "Ouro": Aprendizados, Armadilhas e Bugs Ja Resolvidos ("Gotchas")

1. **Bug de Duplicacao de Instrumentos em Loops (T09 Bug 1 & 2):**
   - *Problema:* Comparar `InstrRun.last_seg == seg.id` faz com que, quando o plano tem dois segmentos no horizonte com o mesmo instrumento (ex: um loop curto), o instrumento seja re-agendado a cada `update`. Alem disso, zerar `last_seg = 0` ao cancelar canais num `replan()` duplica canais do segmento mantido.
   - *Solucao:* Os IDs de segmento crescem monotonicamente. Use `last_seg >= seg.id` para pular instrumentos ja agendados. Em `replan()`, faca clamp de todo `last_seg` para o maior `seg.id` mantido (`last_seg = std::min(last_seg, kept_seg_id)`), e `cancel_channel` nao deve zerar `last_seg`.

2. **Bug de Disparo Retroativo no Re-walk do Horizonte (T09 Bug 3):**
   - *Problema:* Ao estender o plano de timeline (`extend_plan`), re-escanear o ultimo segmento aberto (`HORIZON`) desde o inicio dele (`tl_start`) faz com que uma `transition_region` cujo ponto de quantizacao ficou no passado dispare retroativamente no `dsp_clock` atual.
   - *Solucao:* Ao estender o segmento `HORIZON`, passe `min_tl = old_tl_end`.

3. **Empate de Ordem de Callbacks no Mesmo Frame DSP (T09 Bug 4):**
   - *Problema:* Gerar callbacks de `MARKER`/`BEAT` com limite `[last, now)` enquanto `SOUND_PLAYED` usa `<= now` quebra a ordem deterministica (`STARTED`, `MARKER`, `BEAT`, `SOUND_STOPPED`, `SOUND_PLAYED`, `STOLEN`, `STOPPED`).
   - *Solucao:* Gere `MARKER` e `BEAT` com limite `now + 1` (inclusivo de `now`).

4. **Burst Audivel de Curva de Timeline apos Salto de Loop (Tech Debt T10b-b R6):**
   - *Problema:* Avaliar curvas com `source == TIMELINE` usando `playhead_at(now)` quando um canal e agendado com 200 ms de antecedencia (lookahead) aplica o ganho do fim do loop ao som que vai entrar no inicio do novo loop.
   - *Solucao:* Para canais agendados via lookahead, avalie o valor inicial das curvas `TIMELINE` na **posicao de timeline em que o canal efetivamente comecara a tocar** (`seg.tl_start + frames_to_ms(chan_start_dsp - seg.dsp_start)`).

5. **Churn de Envelope de Fade-Out durante Sweep de Pitch (T09 N1 / T10b-b R2):**
   - *Problema:* Mudar o pitch de um one-shot muda seu fim natural (`nat_end`). Reposicionar o envelope de fade-out a cada update reiniciando de `from = 1.0f` gera cliques se o fade-out ja tiver comecado.
   - *Solucao:* (a) So chame `reapply_ends()` se `nat_end` mudou mais de `512 frames`. (b) Se o fade-out ja tiver comecado no `dsp_clock` atual, inicie a nova rampa a partir do valor atual do envelope.

6. **Lookup Lock-Free de Sistema no `render()` (Tech Debt T02 R1):**
   - *Problema:* Usar `std::mutex` para buscar o ponteiro `System*` dentro de `oae_system_render` viola a regra de tempo real.
   - *Solucao:* Mantenha uma tabela estatica de slots com `std::atomic<System*>` e `std::atomic<uint32_t> generation` para que `oae_system_render` valide o handle e acesse o sistema sem nenhum mutex.

7. **Reamostragem de Sons em Memoria no `miniaudio` (T05 Fix):**
   - *Problema:* Decodificar arquivos em memoria (`create_from_memory`) na taxa nativa do arquivo sem configurar `config.sampleRate = engine_sample_rate` faz sons de 44.1 kHz tocarem fora de tom num engine de 48 kHz.
   - *Solucao:* Inicialize `ma_decoder_config_init(ma_format_f32, engine_channels, engine_sample_rate)` ao decodificar buffers em memoria.

8. **Protecao contra `NaN`/`Inf` e Overflow no Medidor e Sidechain (T07 R9 & R10):**
   - *Solucao:* No `Meter` e no `SidechainTap`, trate qualquer sample `!std::isfinite(s)` como `0.0f` e limite a amplitude maxima medida em `1.0e9f`.

---

9. **Armadilha do PowerShell 5.1 com `vcvarsall.bat` e `2>&1`:**
   - *Armadilha:* No Windows PowerShell 5.1, redirecionar `2>&1` ao invocar `cmd /c "vcvars64.bat && cmake ..."` transforma qualquer escrita em `stderr` (como avisos normais do CMake/Ninja) em `ErrorRecord` terminante se `$ErrorActionPreference = 'Stop'`.
   - *Solução obrigatória:* Em `scripts/build.ps1`, redirecione a saída dentro do próprio `cmd.exe` para um arquivo de log ou execute sem `2>&1` no PowerShell, verificando `$LASTEXITCODE`.

10. **Ciclo de Vida de Handles na Reconstrução de Preview da DAW JUCE (T16e / JP05; referência histórica T16d R7 & R8):**
    - *Armadilha:* Descarregar o banco de preview antigo *antes* de compilar o novo deixa a DAW muda se a compilação falhar; e esquecer de parar a audição ao trocar de projeto mantém o evento antigo tocando.
    - *Solução obrigatória:* Compile o snapshot em background. Só quando o arquivo `.bank` novo estiver pronto e validado, na message thread: (1) salve posição/parâmetros/snapshots ativos, (2) pare e dê `release` nas instâncias ativas, (3) descarregue o banco antigo, (4) carregue o banco novo e (5) readquira os handles e restaure o estado. Ao abrir/criar outro projeto, pare imediatamente a audição e limpe o mapa de parâmetros.


---

## 7. Backlog Mestre de Epicos e Tarefas (Sequencia Continua `T01` a `T25`)

### EPICO 1: Core Engine Nativa & Grafo DSP (`T01` a `T07`)

- **T01 — Repo skeleton, build script & vendored deps**
  - CMake (`C++17`, `C11`, `/W4`), `scripts/build.ps1`, dependencias vendorizadas (`miniaudio`, `stb_vorbis`, `doctest`, `nlohmann/json`), `miniaudio_impl.c`.
  - **AC:** Debug e Release compilam com 0 warnings e `RESULT: OK`.

- **T02 — System lifecycle + headless rendering + SPSC Queue + HandlePool**
  - `HandlePool` geracional (64-bit) e `SpscQueue<T>` lock-free.
  - Funcoes: `oae_get_version`, `oae_result_string`, `oae_init_settings_default`, `oae_system_create`, `oae_system_init`, `oae_system_update`, `oae_system_render`, `oae_system_release`.
  - **AC:** Ciclo create/init/release 100x sem leaks; duplo init -> `ALREADY_INITIALIZED`; chamada antes de init -> `NOT_INITIALIZED`; `render` sem nada tocando retorna `0.0f` exato; stress test SPSC 2 threads com 1M itens preserva ordem.

- **T03 — Sounds & channels (load, play, control)**
  - Funcoes de `oae_sound_*` e `oae_channel_*` basicas (file, memory, WAV/MP3/FLAC/OGG, play, stop, pause, volume, pitch, is_playing, position_ms).
  - **AC:** Todos os 4 formatos decodificam; senoide renderiza com RMS esperado +-5%; volume 0.5 reduz RMS pela metade +-5%; pitch 2.0 reduz duracao pela metade; handle de canal apos terminar -> `INVALID_HANDLE`.

- **T04 — Bus / mixer graph**
  - Hierarquia de buses ate profundidade 32 (`oae_system_get_master_bus`, `oae_bus_create`, `oae_system_get_bus`, `set/get_volume`, `set/get_mute`, `set/get_paused`, `stop_all`, `release`).
  - **AC:** Canal em bus filho a 0.5 sob pai a 0.5 -> RMS * 0.25 +-5%; mutar pai silencia subarvore; destruir bus invalida handles de toda a subarvore e canais.

- **T05 — Voice management & virtualization**
  - Teto rigido `max_physical_voices`, prioridades `0..256` (`set/get_priority`, `is_virtual`, `get_channels_playing`). Ranking em `update`: `(priority ASC, audibility DESC, age ASC)`. Relogio logico para vozes virtuais.
  - **AC:** Com limite 4 e 10 canais, exatamente 4 sao reais; reduzir volume de canal real promove um virtual mais alto no proximo `update`; posicao logica virtual avanca dentro de +-50 ms; zero alocacoes na thread de audio.

- **T06 — Stereo pan, fades & sample-accurate scheduling**
  - Pan constant-power + de-click de 128 frames (`set/get_pan`), fades no relogio DSP (`fade_to`, `get_fade_gain`, `stop_with_fade` com curvas `LINEAR`, `LOG`, `SCURVE`), agendamento exato (`get_dsp_clock`, `play_sound_at`, `play_sound_delayed`).
  - **AC:** Pan -1 silencia canal R (< -60 dB / 0.0f exato apos rampa), pan 0 mantem L/R identicos, constant-power em +-0.5 (+-0.5 dB); fade de 500 ms tem RMS esperado no meio +-10%; dois sons agendados com 120 ms (5760 frames a 48 kHz) iniciam com precisao de 1 frame.

- **T07 — Bus effects (DSP chain) & metering**
  - Cadeia pre/pos-fader de ate 8 efeitos por posicao (`LIMITER`, `COMPRESSOR` com sidechain, `EQ3`, `LOWPASS`, `HIGHPASS`, `REVERB`, `DELAY`), `set/get_param`, `set/get_bypass`, `set/get_wet_dry`, `set_sidechain`, `release`, e `oae_bus_get_meter`.
  - **AC:** Limiter no master mantem pico <= -0.1 dBFS com 10 senoides full-scale somadas; compressor com sidechain de `bus:/vo` reduz `bus:/music` em >= 6 dB e recupera apos release; bypass e wet 0 / dry 1 sao bit-exatos; LPF em 1 kHz atenua 5 kHz em >= 12 dB; medidor bate com RMS offline dentro de +-0.5 dB.

### EPICO 2: Modelo de Eventos, Linha do Tempo, Parametros, Automacao & Snapshots (`T08` a `T11`)

- **T08 — Studio events (model, instances, instruments)**
  - `ModelSetBuilder` / `EventBuilder`, instrumentos `SINGLE`, `MULTI` (`RANDOM`, `RANDOM_NO_REPEAT`, `SEQUENTIAL`, `SHUFFLE`), `EVENT`, `COMMAND`, `SILENCE`; roubo de instancias (`OLDEST`, `QUIETEST`, `NONE`), `cooldown_ms`, `priority`, `stop_fadeout_ms`.
  - **AC:** Evento de 2 tracks toca ambos; instrumento em 250 ms inicia em 250 ms; random ponderado dentro de +-5% em 10k picks (PCG32 seeded); `no_repeat` nunca repete; `shuffle` toca todos 1x por ciclo; `max_instances=3` mantem 3 vivos.

- **T09 — Timeline logic & callbacks**
  - `MARKER`, `TEMPO`, `LOOP_REGION`, `SUSTAIN_POINT`, `TRANSITION_MARKER`, `TRANSITION_REGION` (`IMMEDIATE`, `BEAT`, `BAR` + crossfade), `key_off`, `get/set_timeline_position_ms`, e fila de callbacks na game thread (`STARTED`, `STOPPED`, `MARKER`, `BEAT`, `STOLEN`, `SOUND_PLAYED`, `SOUND_STOPPED`).
  - **AC:** `MARKER` reporta `dsp_clock` exato; `BEAT` a 120 BPM chega a cada 24000 frames; transicao `BAR` pousa exatamente na barra; loop sustentado termina a iteracao apos `key_off`; callbacks nunca rodam na thread de audio.

- **T10a — Parameters, trigger conditions & probability**
  - Parametros locais e globais (`CONTINUOUS`, `DISCRETE`, `LABELED`), `seek_speed`, `seek_speed_down`, condicoes (`0..4` termos AND + `probability_pct`), e as 6 funcoes publicas de get/set parameter.
  - **AC:** Clamp na faixa; label seleciona instrumento; probabilidade de 50% dispara 50% +-5% em 10k triggers; seek speed interpola pelo tempo DSP; condicao controla loops e transicoes.

- **T10b-a — RT foundations (smoothing, de-click) + RANDOM/AHDSR modulators**
  - Suavizacao de 20 ms nos filtros/EQ3, de-click de 128 frames em pan/bus volume, moduladores `RANDOM` e `AHDSR` (com release em `stop(ALLOW_FADEOUT)` e `key_off`).
  - **AC:** AHDSR release decai no tempo configurado +-10% apos stop; `RANDOM` respeita faixa em 10k triggers; salto de LPF Q=10 de 20 kHz para 10 Hz nao explode.

- **T10b-b — Automation curves (parameter/timeline -> volume, pitch, pan, effects)**
  - Curvas de 1 a 32 pontos (`LINEAR`, `HOLD`, `EASE_IN`, `EASE_OUT`, `S_CURVE`) por parametro ou timeline, heranca em eventos aninhados, `EffectAutomation` de buses, e ajuste dinamico do fim natural de one-shots quando o pitch muda.
  - **AC:** `RPM` 0->8000 altera o pitch seguindo a curva (medido por zero-crossing em 3 pontos); automacao de volume para -80 dB gera silencio exato; automacao de timeline acompanha saltos de loop.

- **T11 — Snapshots & VCAs**
  - Snapshots (`OVERRIDE` e `BLENDING`, prioridade, intensidade parametrizada, fade-in/out, alvos de bus e efeito) e VCAs (`oae_studio_get_vca`, `set/get_volume`).
  - **AC:** Snapshot `"DuckMusic"` reduz `bus:/music` em 9 dB a 100% e 4.5 dB a 50%; dois snapshots `OVERRIDE` resolvem por prioridade; parar snapshot restaura valores originais; VCA a 0.5 reduz RMS pela metade sem alterar `oae_bus_get_volume`.

### EPICO 3: Compilacao de Bancos, CLI & Runtime I/O (`T12a` a `T14c`)

- **T12a — Bank format spec + writer/reader (core chunks: `STRS`, `BUSS`, `EVTS`, `SMPL`, `DATA`)**
  - Implementar serializador e parser dos 5 chunks core conforme Secao 4, mais `oae_studio_load_bank_file/memory` e `unload_bank`.
  - **AC:** Round-trip de escrita e leitura preserva igualdade exata; magic/CRC/alinhamento/versao invalidos retornam `OAE_ERR_FORMAT`; chunk desconhecido com CRC valido e ignorado.

- **T12b — Bank remaining chunks (`LOGC`, `PARM`, `SNAP`, `VCAS`), fuzz & load/unload**
  - Serializacao e leitura dos 4 chunks de extensao conforme Secao 4.3.
  - **AC:** Round-trip completo de todo o modelo T08-T11; teste de fuzzing com 10.000 mutacoes aleatorias nunca da crash; descarregar banco com instancias ativas para os sons e invalida os handles.

- **T13 — Bank compiler CLI (`oae_bankc`, JSON -> `.bank` + `.bank.catalog.json`)**
  - Executavel `oae_bankc <project.json> -o <out.bank>` implementando o schema JSON v1 e publicacao atomica via `.bankc-stage`.
  - **AC:** Compila projeto de teste; diagnostica erros de referencia com JSON path e `exit code != 0`; banco gerado carrega e toca.

- **T14a — C API audit + garbage-args fuzz + DLL exports**
  - Auditoria de todas as funcoes da API C, `oae_get_struct_sizes`, verificacao de exports da DLL (`dumpbin /exports`).
  - **AC:** Fuzz test chamando todas as funcoes da API C com argumentos aleatorios/invalidos (10k iteracoes) sem crash; apenas simbolos `oae_*` exportados.

- **T14b — Async bank loading + sample data load/unload**
  - `oae_studio_load_bank_file_ex`, `load_bank_memory_ex` (`OAE_LOAD_BANK_NONBLOCKING`), `get_bank_loading_state`, `oae_event_description_load_sample_data`, `unload_sample_data`.
  - **AC:** Load assincrono nunca bloqueia `update` por > 1 ms; estados `LOADING -> LOADED | FAILED` observaveis; pinagem de samples compartilhados entre eventos funciona sem leaks.

- **T14c — End-to-end native test + API reference**
  - Teste E2E nativo compilando JSON com `oae_bankc`, carregando via API C, tocando evento, mudando parametro, recebendo callback `MARKER`, ativando snapshot e verificando audio renderizado. Documentacao completa da API.
  - **AC:** Teste E2E passa em Debug e Release; todas as 100 funcoes documentadas.

### EPICO 4: Integracao Unity (`T15a`, `T15b`)

- **T15a — Unity: P/Invoke layer, wrappers & deploy script**
  - Projeto Unity + pacote `com.openaudio.core`, `OaeNative.cs`, wrappers C#, `scripts/deploy_unity.ps1` e `scripts/unity_test.ps1`.
  - **AC:** Testes EditMode verificam versao, tamanhos de structs (`Marshal.SizeOf` == `oae_get_struct_sizes`), e renderizacao headless nao-silenciosa.

- **T15b — Unity: runtime components, callbacks & pause**
  - `OaeAudioManager`, `StudioEventEmitter`, `StudioBankLoader`, `StudioListener`, trampolim `[MonoPInvokeCallback]` para eventos C# na main thread, e pausa automatica em `OnApplicationPause/Focus`.
  - **AC:** Callback `MARKER` chega ao C# na main thread; componentes iniciam/param eventos conforme ciclo de vida da Unity.

### EPICO 5: OpenAudio Studio Desktop DAW em C++17 / JUCE 8 (`T16a` a `T16f`)

> **Importante:** As tarefas `T16a` a `T16f` correspondem diretamente aos 6 modulos da DAW Desktop em **C++17 + JUCE 8.0.12** (`JP01` a `JP06`), mantendo a numeracao sequencial do projeto sem usar Unity UI Toolkit.

- **T16a (JP01) — Studio: Standalone JUCE Target & Native Integration Proof**
  - Target CMake `oae_studio` com JUCE 8.0.12, integrando `oae_audio.dll` e `oae_bankc.exe`, mais modo `--smoke-headless`.
  - **AC:** Executavel inicia no Windows x64 sem depender da Unity; `--smoke-headless` compila banco fixture, toca evento e valida audio nao-zero.

- **T16b (JP02) — Studio: Portable Projects, Relinking & Async Bank Publication**
  - Criacao, abertura e salvamento atomico de `project.json` v1; importacao de audio com caminhos relativos; relink de arquivos ausentes; build assincrono de snapshot imutavel.
  - **AC:** Mover pasta do projeto para caminho com espacos e Unicode preserva abertura e build; falha de save/build preserva arquivos validos anteriores.

- **T16c (JP03) — Studio: Authoring Model, Hierarchical Browser & Typed Inspector**
  - Browser hierarquico em arvore (`Events`, `Snapshots`, `Audio Assets`, `Buses & VCAs`, `Banks`) com busca e CRUD completo (F2, Ctrl+D, Del) atualizando referencias cruzadas; Inspector 100% tipado para todos os campos do schema v1; Undo/Redo por gesto.
  - **AC:** Teste automatizado cria evento de 2 tracks com multi-instrument, parametro rotulado, condicao, curva de automacao e efeito de bus pelos controles tipados, testa rename/delete com integridade de referencias e faz undo/redo.

- **T16d (JP04) — Studio: Timeline, Waveforms, Ruler Logic & Clip Editing**
  - Regua de tempo (segundos e batidas/compassos), snap magnetico, zoom (`Ctrl+Wheel`), cabecalhos de track com volume e Mute/Solo, decodificacao assincrona de waveforms Peak/RMS em background, edicao multi-track de clipes, marcadores/regioes na regua e curvas de automacao na faixa.
  - **AC:** Teste `--smoke-project` valida edicao de clipes, snap, alinhamento de waveform com pitch/offset/loop, Mute/Solo de tracks e persistencia.

- **T16e (JP05) — Studio: Audition Transport, Debounced Preview Rebuild & Mixer Parity**
  - Barra de transporte completa (Play, Pause/Resume, Stop, Loop, Seek, KeyOff, timecode, controles de parametros), rebuild automatico debounced (600 ms) quando `ContentRevision` muda (preservando posicao, parametros e snapshots ativos), e Mixer com strips de Buses/VCAs, regra de Solo hierarquica e medidores Peak/RMS em tempo real.
  - **AC:** Editar clipe durante playback reconstroi preview sem reiniciar o Studio; fechar projeto com build em andamento nao aplica banco antigo.

- **T16f (JP06) — Studio: Packaged Parity, Shell Persistence & Unity `[EventRef]` Picker**
  - Menu bar, splitters redimensionaveis persistidos em `oae_studio_settings.json`, console de mensagens com severidade, `scripts/build_studio.ps1`, e `EventRefDrawer` na Unity consumindo o `.bank.catalog.json` com cache e validacao CRC-32.
  - **AC:** Bundle Release passa em `--smoke-headless`, `--smoke-project` e `--smoke-device`; seletor `[EventRef]` na Unity detecta catalogo obsoleto por CRC-32/timestamp e nao re-escaneia disco a cada repaint.

### EPICO 4 (Conclusao do MVP Vertical Slice): Demo & Validacao Ponta-a-Ponta (`T17`)

- **T17 — End-to-end demo scene + Unity automated tests + docs**
  - Cena demonstrativa na Unity integrando bancos exportados pelo Studio JUCE com eventos em loop, disparos panoramizados L->R, camadas adaptativas, envelopes `sustain`/`key_off`, snapshot `"DuckMusic"`, transição musical quantizada em compasso (`BAR`) e controle via VCA.
  - **AC:** `scripts/unity_test.ps1` passa todos os testes EditMode e PlayMode; fluxo completo Studio -> `.bank` -> Unity validado.

### EPICO 6: Ponte Semantica & Automacao por IA — MCP Server (`MCP-01` a `MCP-05`)

- **MCP-01:** Atribuir `ComponentID` semantico deterministico a todos os controles da DAW JUCE.
- **MCP-02:** Servidor MCP JSON-RPC 2.0 via `stdio` (`--mcp`) ou socket `127.0.0.1:7451`.
- **MCP-03:** Tools `studio_trigger_control`, `studio_select_item`, `studio_move_clip`, `studio_transport`, `studio_set_mixer_fader`, `studio_build_bank`.
- **MCP-04:** Resources e diffing automatico RFC 6902 (`nlohmann::json::diff`) a cada mutacao + telemetria de medidores.
- **MCP-05:** Runner `--smoke-mcp` pilotando a DAW 100% via protocolo MCP.

### EPICO 7: Monitoramento Acustico Avancado & Balistica (`VU-01` a `VU-04`)

- **VU-01:** Seletor de escalas no Mixer (`dBFS Nominal`, `K-12`, `K-14`, `K-20`, `EBU R128 LUFS`).
- **VU-02:** Balistica IEC 60268-17 (ataque instantaneo, decaimento `20 dB/s`, integracao RMS `300 ms`, Peak Hold `1.2 s`).
- **VU-03:** Processador ITU-R BS.1770 / EBU R128 (filtro K-Weighting + Momentary `400 ms`, Short-Term `3 s`, Integrated com gating `-70 LUFS` / `-10 dB` e True Peak `dBTP` com oversampling `4x`).
- **VU-04:** Testes de calibracao (senoide a `-14 dBFS` marca `0.0 VU` exato no modo `K-14`).

### ÉPICO 11 — OpenAudioStudio V1 para jogos 2D (`V1-01` a `V1-06`)

- **V1-01:** Paridade entre autoria, banco e jogo e diagnóstico de publicação.
- **V1-02:** Emissores, zonas e controles de áudio 2D na Unity.
- **V1-03:** Música adaptativa sincronizada ao relógio de áudio.
- **V1-04:** Streaming de música e ambiência no conteúdo publicado.
- **V1-05:** Mix por categorias e diagnóstico em jogo.
- **V1-06:** Cena 2D de aceite em uma build Unity Windows x64, com resultados reproduzíveis.

O épico segue a medição do Épico 7. As tarefas, os gates e o roteiro de aceite estão na [especificação da V1](docs/specs/epic-v1-audio-2d.md). O cenário evolui a cena demonstrativa de T17.

### EPICO 8: Conectividade em Tempo Real / Live-Linking (`LIVE-01` a `LIVE-04`)

- **LIVE-01:** Servidor TCP Bridge na porta `7450` dentro do Studio com handshake de versao e `bank_crc32`.
- **LIVE-02:** Cliente `LiveClient.cs` no pacote Unity para conexao em PlayMode / Development Builds.
- **LIVE-03:** Sincronizacao bidirecional (`< 10 ms`) de faders, parametros e curvas entre Studio e jogo.
- **LIVE-04:** Auditoria remota no `AudioListener` do jogo com fallback automatico para modo offline.

### EPICO 9: Expansao de Audio Interativo Avancado (`T18` a `T25`)

- **T18 — Audio sends and returns:** Envios pre/pos-fader para buses de retorno com rejeicao de ciclos e remocao lock-free.
- **T19 — Per-event track mixing and effects:** Sub-mixer por instancia de evento com efeitos por track e gerenciamento de cauda.
- **T20 — Asynchronous instruments:** Modo assincrono para instrumentos (`SINGLE`/`MULTI`) que continuam tocando atraves de saltos de loop/transicao.
- **T21 — Parameter sheets and region triggering:** Abas de evento guiadas por eixo de parametro continuo/discreto/rotulado.
- **T22 — Programmer instruments and audio tables:** Instrumentos resolvidos em runtime por chave de tabela de audio para dialogos/localizacao.
- **T23 — Unity Timeline integration:** Tracks e clips customizados para `PlayableDirector` da Unity.
- **T24 — Unity Addressables and AssetBundles:** Carregamento assincrono de bancos via Addressables/AssetBundles com contagem de referencias.
- **T25 — Nested/overlapping loops and event pitch timing:** Regioes de loop aninhadas/sobrepostas e modo de pitch de evento que escala a velocidade do playhead da timeline junto com o pitch acustico.

### EPICO 10: Multiplataforma & Audio Espacial 3D (`Milestone 2 & 3`)

- **M2-P01 — Port Linux x64:** Biblioteca compartilhada `.so` (ALSA/PulseAudio).
- **M2-P02 — Port iOS / iPadOS:** Biblioteca estatica `.a` (`DllImport("__Internal")`, callbacks AOT-safe, Core Audio).
- **M2-P03 — Port WebGL:** Build Emscripten + backend Web Audio API.
- **M2-ENC — Per-platform encoding no `oae_bankc`:** Transcodificacao offline para Vorbis / ADPCM.
- **M2-PROF — Real-Time Profiler:** Telemetria de CPU, memoria de samples, vozes reais/virtuais e instancias.
- **M3-3D — 3D Spatialization:** Posicionamento 3D, atenuacao por distancia, curvas de rolloff, Doppler, multiplos listeners e oclusao.



---

## 8. Requisitos Detalhados Recuperados do Original

Esta seção preserva integralmente o escopo e os critérios de aceitação detalhados do backlog original, complementando os resumos da seção 7. A ordem de leitura dos blocos não define execução: vale exclusivamente a sequência do início deste documento (DAW antes de T17; depois MCP, VU, V1 2D, LIVE e expansões). JP01–JP06 são aliases de T16a–T16f, não tarefas adicionais. O seletor Unity `[EventRef]` é entregue em T16f conforme o backlog consolidado, mesmo quando mencionado no escopo histórico T15.

Para economizar contexto, consulte apenas o bloco da tarefa corrente. Havendo divergência técnica não resolvida pelas decisões explícitas desta consolidação, pare e pergunte; nenhum critério detalhado deve ser descartado silenciosamente.

### ÉPICO 1: Core Engine Nativa & Grafo DSP (`T01` a `T07`)

#### T01 — Repo skeleton, build script & vendored deps
- **Escopo:** Configurar `CMakeLists.txt` (C++17, `/W4` no MSVC, `-Wall -Wextra` no GCC/Clang), `scripts/build.ps1` (flags `-Config Debug|Release`, `-NoTests`, `-Clean`, `-TestFilter`, `-Full`), e adicionar em `native/external/`: `miniaudio.h`, `stb_vorbis.c`, `doctest.h`, `nlohmann/json.hpp`. Criar `native/src/miniaudio_impl.c`.
- **AC:** `scripts/build.ps1` em Debug e Release imprime `RESULT: OK` com 0 warnings.

#### T02 — System lifecycle + headless rendering + SPSC Queue + HandlePool
- **Escopo:**
  - `HandlePool` geracional (64-bit: index + gen + type) e `SpscQueue<T>` lock-free em `native/src/common/`.
  - C API: `oae_get_version`, `oae_result_string`, `oae_init_settings_default`, `oae_system_create`, `oae_system_init`, `oae_system_update`, `oae_system_render`, `oae_system_release`.
  - Suporte a `OAE_INIT_NO_DEVICE` puxando frames PCM via `ma_engine_read_pcm_frames`.
  - Lookup de sistema lock-free no caminho de renderização (evita Tech Debt T02 R1).
- **AC:** Ciclo create/init/release 100x sem leaks; duplo init  `ALREADY_INITIALIZED`; chamada antes de init  `NOT_INITIALIZED`; ponteiros nulos  `INVALID_PARAM`; `render` sem nada tocando retorna silêncio exato (`0.0f`); `render` sem `NO_DEVICE`  `OAE_ERR_DEVICE`; teste de stress SPSC com 2 threads e 1 milhão de itens preserva ordem exata.

#### T03 — Sounds & channels (load, play, control)
- **Escopo:**
  - `oae_sound_create_from_file`, `oae_sound_create_from_memory`, `oae_sound_release`, `oae_sound_get_length_ms` (flags: `DEFAULT`, `LOOP`, `STREAM`).
  - `STREAM` é exclusivo de arquivos. Em memória, retorna `OAE_ERR_INVALID_PARAM`. Liberar o som invalida seu handle e mantém os canais ativos até terminarem. Ver [contrato da T03](docs/specs/T03-sound-contract.md).
  - `oae_system_play_sound(sys, sound, bus, paused, &ch)`; controles de canal: `stop`, `set_paused/get_paused`, `set_volume/get_volume`, `set_pitch/get_pitch`, `is_playing`, `get_position_ms/set_position_ms`.
  - Suporte a WAV, MP3, FLAC, OGG Vorbis. Canais one-shot finalizados são reciclados em `update` (invalidando o handle).
- **AC:** Todos os 4 formatos decodificam; senoide tocada renderiza com RMS esperado $\pm 5\%$; volume `0.5` reduz RMS pela metade $\pm 5\%$; pausado  silêncio; pitch `2.0` reduz duração pela metade; handle de canal após término  `INVALID_HANDLE`; `NaN` ou negativo em volume/pitch  `INVALID_PARAM`.

#### T04 — Bus / mixer graph
- **Escopo:**
  - `oae_system_get_master_bus`, `oae_bus_create(sys, name, parent, &bus)`, `oae_system_get_bus(sys, "bus:/sfx/ui", &bus)`, `set_volume/get_volume`, `set_mute/get_mute`, `set_paused/get_paused`, `stop_all`, `release`.
  - Hierarquia sobre `ma_sound_group` + `StripNode` customizado. Profundidade máxima de 32 níveis abaixo do master (`> 32`  `INVALID_PARAM`). `bus_release` destrói recursivamente a subárvore e seus canais.
  - O Master tem caminho canônico `bus:/`. `oae_bus_stop_all` para os canais de toda a subárvore. Ver [contrato da T04](docs/specs/T04-bus-contract.md).
- **AC:** Canal em bus filho a `0.5` sob pai a `0.5`  RMS $\times 0.25 \pm 5\%$; mutar pai silencia filhos; destruir bus invalida handles de toda a subárvore e canais; busca por path case-sensitive funciona.

#### T05 — Voice management & virtualization
- **Escopo:**
  - Teto rígido `max_physical_voices`. Prioridade por canal `0..256` (0 = mais alta, 128 = padrão): `oae_channel_set_priority/get_priority`, `oae_channel_is_virtual`, `oae_system_get_channels_playing(sys, &total, &real)`.
  - Quando todos os slots reais estão ocupados, novos canais já nascem virtuais; `update` reclassifica por `(priority ASC, audibility DESC, age ASC)` e promove/rebaixa sem alocar memória na thread de áudio. Vozes virtuais avançam posição lógica com o tempo (respeitando pitch, loop e pause).
- **AC:** Com limite 4 e 10 canais, exatamente 4 são reais (os de maior prioridade/volume); reduzir volume de um canal real troca-o por um virtual mais alto após `update`; posição lógica de canal virtual avança dentro de 50ms; zero alocações na thread de renderização.

#### T06 — Stereo pan, fades & sample-accurate scheduling
- **Escopo:**
  - `oae_channel_set_pan/get_pan`, `oae_bus_set_pan/get_pan` (lei de potência constante + rampa de de-click de 128 frames).
  - `oae_channel_fade_to`, `oae_bus_fade_to` (`LINEAR`, `LOG`, `SCURVE`), `get_fade_gain`, `oae_channel_stop_with_fade`.
  - `oae_system_get_dsp_clock`, `oae_system_play_sound_at`, `oae_system_play_sound_delayed` (limitado a `now + 1h`).
  - `stop_with_fade` usa LINEAR. Se `play_sound_at` receber um frame passado, começa no próximo frame renderizável. Ver [contrato da T06](docs/specs/T06-timing-contract.md).
- **AC:** Pan `-1`  canal R silêncio exato; pan `0`  L/R idênticos; potência constante em 0.5 (0.5dB); fade linear de 500 ms tem RMS esperado no meio $\pm 10\%$; `stop_with_fade` termina em silêncio e invalida o handle no próximo `update`; dois canais agendados com 120 ms (5760 frames a 48 kHz) de diferença iniciam com precisão amostral.

#### T07 — Bus effects (DSP chain) & metering
- **Escopo:**
  - Cadeia de até 8 efeitos pré-fader e 8 pós-fader por bus: `oae_bus_add_effect`, `oae_bus_add_effect_ex`, `oae_effect_set_param/get_param`, `set_bypass/get_bypass`, `set_wet_dry/get_wet_dry`, `set_sidechain`, `release`.
  - Efeitos: `LIMITER` (look-ahead 1.5 ms, brickwall), `COMPRESSOR` (com sidechain tap de 1 bloco de outro bus), `EQ3` (Linkwitz-Riley 4ª ordem), `LOWPASS`, `HIGHPASS` (Biquad 12 dB/oct com suavização de 20 ms), `REVERB` (Freeverb), `DELAY` (até 2s pré-alocado).
  - Medidor pós-fader/pós-mute: `oae_bus_get_meter(bus, &meter)` com Peak e RMS por canal.
  - O compressor capta o sidechain após fader, efeitos e mute. Usa o pico do bloco anterior do bus fonte. Ver [contrato da T07](docs/specs/T07-effects-contract.md).
- **AC:** Limiter no master mantém pico -0.1dBFS com 10 senoides full-scale somadas; compressor com sidechain de `bus:/vo` reduz `bus:/music` em 6dB e recupera após release; bypass e `wet=0, dry=1` são bit-exatos; LPF em 1 kHz atenua senoide de 5 kHz em 12dB; medidor bate com RMS offline dentro de 0.5dB; salto de LPF Q=10 de 20 kHz para 10 Hz permanece suave e limitado; zero alocações na thread de áudio.

---

### ÉPICO 2: Modelo de Eventos, Linha do Tempo, Parâmetros, Automação & Snapshots (`T08` a `T11`)

#### T08 — Studio events (model, instances, instruments)
- **Escopo:**
  - Modelo imutável em memória (`ModelSet`, `EventDescriptionModel`, `Track`, `Instrument`) + builder C++ interno (`ModelSetBuilder`, `EventBuilder`).
  - Tipos de instrumento: `SINGLE`, `MULTI` (`RANDOM`, `RANDOM_NO_REPEAT`, `SEQUENTIAL`, `SHUFFLE` com escopo `GLOBAL` ou `INSTANCE` e entradas ponderadas), `EVENT` (evento aninhado até profundidade 8), `COMMAND` (`START_EVENT`, `STOP_EVENT`, `SNAPSHOT_ON`, `SNAPSHOT_OFF`), `SILENCE`.
  - Limite de instâncias por evento (`max_instances` + steal `OLDEST | QUIETEST | NONE`), `cooldown_ms`, `priority`, `stop_fadeout_ms`.
  - API C: `oae_studio_get_event`, `oae_studio_play_one_shot`, `oae_event_description_create_instance`, `get_length_ms`, `is_oneshot`, `get_instance_count`, e funções de instância (`start`, `stop`, `release`, `get_playback_state`, `get_description`, `set/get_volume`, `set/get_pitch`, `set/get_pan`).
- **AC:** Evento de 2 faixas toca ambas; instrumento em 250 ms inicia em 250 ms; random ponderado dentro de $\pm 5\%$ em 10k triggers (PCG32 com seed fixa); `no_repeat` nunca repete; `shuffle` toca todos uma vez por ciclo; `max_instances=3` mantém 3 vivos em todos os modos de steal; `cooldown=100ms` descarta triggers na janela retornando `OAE_OK`.

#### T09 — Timeline logic & callbacks
- **Escopo:**
  - `LogicItem` no modelo: `MARKER`, `TEMPO` (BPM + fórmula de compasso), `LOOP_REGION` (contagem de repetições, `until_key_off`, condição), `SUSTAIN_POINT`, `TRANSITION_MARKER`, `TRANSITION_REGION` (quantização `IMMEDIATE | BEAT | BAR` + `crossfade_ms`).
  - Planejador de segmentos de timeline na game thread com *commit margin* (`max(10 ms, 2 device periods)`) e *catch-up* analítico em caso de travamento (hitch) da game thread.
  - Fila pré-alocada de callbacks (capacidade 4096; reserva de emergência acima de 3072 para nunca descartar `STARTED`, `STOPPED`, `STOLEN`) despachada em `update`.
  - API C: `oae_event_instance_set_callback`, `oae_event_instance_key_off`, `get/set_timeline_position_ms`.
- **AC:** Callback `MARKER` reporta `dsp_clock` exato; `BEAT` a 120 BPM chega a cada 24000 frames exatos; transição `BAR` pousa exatamente na barra; loop sustentado termina a iteração atual após `key_off` e segue para a cauda; callbacks nunca rodam na thread de áudio.

#### T10a — Parameters, trigger conditions & probability
- **Escopo:**
  - Parâmetros locais (por instância) e globais (por sistema): `CONTINUOUS`, `DISCRETE`, `LABELED` (até 64 rótulos), com `min`, `max`, `default_value`, `seek_speed` e `seek_speed_down`.
  - Condições de disparo (`Condition`: até 4 termos `ParamRef in [min, max]` unidos por AND + `probability_pct` $0..100\%$) aplicadas a instrumentos, regiões de loop e transições.
  - API C: `oae_event_instance_set_parameter_by_name`, `set_parameter_by_name_with_label`, `get_parameter_by_name`, `oae_studio_set_global_parameter_by_name`, `set_global_parameter_by_name_with_label`, `get_global_parameter_by_name`.
- **AC:** Valores sofrem clamp para `[min, max]`; parâmetro rotulado seleciona o instrumento correspondente; probabilidade de 50% dispara em $50\% \pm 5\%$ em 10k triggers; parâmetro global afeta todas as instâncias; `seek_speed` interpola deterministicamente pelo tempo DSP; condição de parâmetro controla loops e transições da T09.

#### T10b-a & T10b-b — Modulators (RANDOM, AHDSR) & Automation Curves
- **Escopo:**
  - Moduladores (`Modulator`): `RANDOM` (limitado por propriedade: vol 80dB, pitch 48st, pan 1) e `AHDSR` (`attack_ms`, `hold_ms`, `decay_ms`, `sustain`, `release_ms`, `release_on_key_off`) avaliado amostra-a-amostra no `StripNode`.
  - Curvas de automação (`Automation` e `EffectAutomation`): curvas por partes de 1 a 32 pontos (`LINEAR`, `HOLD`, `EASE_IN`, `EASE_OUT`, `S_CURVE`) mapeando parâmetro ou posição da timeline para volume (`dB`, -80dB=silêncioexato), pitch (`semitons`), pan ou parâmetros de filtros/EQ3 de buses.
- **AC:** Release de AHDSR audível após `stop(ALLOW_FADEOUT)` decaindo no tempo configurado $\pm 10\%$; `RANDOM` respeita faixa em 10k triggers; `RPM` 08000 move pitch ao longo da curva (verificado por taxa de zero-crossing em 3 pontos); automação de volume para `-80 dB` zera amostras exatamente; automação de timeline acompanha saltos de loop sem burst audível (usando posição de início do canal).

#### T11 — Snapshots & VCAs
- **Escopo:**
  - Snapshots (`snapshot:/path`): modelados como eventos (`EventKind::SNAPSHOT`), modos `OVERRIDE` ou `BLENDING`, `priority`, `fade_in_ms`, `stop_fadeout_ms`, parâmetro opcional de `intensity` ($0..100\%$) e lista de alvos (volume relativo de bus em dB ou parâmetro de efeito de bus).
  - VCAs (`vca:/path`): `oae_studio_get_vca`, `oae_vca_set_volume`, `oae_vca_get_volume` (multiplicadores lineares de ganho 0..1 sobre buses e eventos associados, sem alterar o valor lido dos faders).
- **AC:** Snapshot `"DuckMusic"` reduz `bus:/music` em 9 dB a 100% de intensidade e 4.5 dB a 50%; dois snapshots `OVERRIDE` resolvem por prioridade; parar snapshot restaura valores originais após fade-out; VCA a `0.5` reduz RMS pela metade mantendo `oae_bus_get_volume` inalterado.

---

### ÉPICO 3: Compilação de Bancos, CLI & Runtime I/O (`T12a` a `T14c`)

#### T12a & T12b — Bank format writer/reader + Fuzzing + Load/Unload
- **Escopo:**
  - Implementar serializador (`bank_writer.*`) e parser (`bank_reader.*`) para todos os 9 chunks da Seções 4.1–4.3 (`STRS`, `BUSS`, `EVTS`, `SMPL`, `DATA`, `LOGC`, `PARM`, `SNAP`, `VCAS`).
  - API C: `oae_studio_load_bank_memory`, `oae_studio_load_bank_file`, `oae_studio_unload_bank`.
- **AC:** Round-trip `write -> read` preserva igualdade exata de todo o modelo T08–T11; 10.000 mutações aleatórias de bytes (fuzzing) nunca causam crash e retornam `OAE_ERR_FORMAT`; descarregar banco com instâncias tocando para todas as instâncias e invalida seus handles.

#### T13 — Bank compiler CLI (`oae_bankc`)
- **Escopo:**
  - Executável `native/tools/bankc/oae_bankc.exe <project.json> -o <out.bank>`.
  - Faz parse do schema JSON v1 (Seção 4.4), resolve caminhos de áudio relativos ao JSON, valida referências cruzadas e ciclos, e publica atomicamente `<out.bank>` e `<out.bank.catalog.json>`.
- **AC:** Projeto de exemplo compila e toca; referências inválidas (arquivo ausente, bus/marker/parâmetro/label inexistente, ciclo de eventos aninhados) produzem mensagem de erro legível com o JSON path e `exit code != 0`.

#### T14a, T14b & T14c — C API Hardening, Async Bank Loading & Sample Data Management
- **Escopo:**
  - `oae_get_struct_sizes` para verificação de layout FFI.
  - Carregamento assíncrono de bancos: `oae_studio_load_bank_file_ex`, `oae_studio_load_bank_memory_ex` (`OAE_LOAD_BANK_NONBLOCKING`), `oae_studio_get_bank_loading_state`.
  - Gerenciamento explícito de samples por evento: `oae_event_description_load_sample_data`, `oae_event_description_unload_sample_data` (contagem de referências/pins compartilhada entre eventos do mesmo banco).
  - Verificação de símbolos exportados na DLL (`dumpbin /exports`: apenas funções `oae_*`).
- **AC:** Fuzzing de API (10k chamadas com argumentos aleatórios/lixo) nunca causa crash; carregamento `NONBLOCKING` nunca bloqueia `update` por >1ms; iniciar evento não-bloqueante antes de `load_sample_data` retorna `OAE_ERR_INVALID_STATE` e após preload renderiza áudio; teste E2E nativo completo passa em Debug e Release.

---

### ÉPICO 4: Integração Unity & Workflow de Jogo (`T15a`, `T15b`, `T17`)

#### T15a & T15b — Unity UPM Package (`com.openaudio.core`), P/Invoke & Runtime Components
- **Escopo:**
  - `OaeNative.cs` (`[DllImport("oae_audio", CallingConvention = CallingConvention.Cdecl)]`), structs blittable espelhadas e verificadas contra `oae_get_struct_sizes`.
  - Wrappers C# seguros (`OaeSystem`, `EventDescription`, `EventInstance`, `Bus`, `Vca`, `Bank`).
  - Trampolim estático `[MonoPInvokeCallback]` para callbacks nativos (compatível com IL2CPP/AOT).
  - Componentes: `OaeAudioManager` (singleton `DontDestroyOnLoad`, chama `update`, pausa em perda de foco), `StudioBankLoader`, `StudioEventEmitter`, `StudioListener`.
  - `EventRefAttribute` + `EventRefDrawer` com cache de catálogo invalidado por timestamp/tamanho e verificação de CRC-32 contra o `.bank`.
- **AC:** Testes EditMode e PlayMode em batchmode verificam tamanhos de structs, carregamento de banco, reprodução headless não-silenciosa, recebimento de callback `MARKER` na main thread da Unity, e cache do picker `[EventRef]`.

#### T17 — End-to-End Demo Scene & Game Flow
- **Escopo:**
  - Cena demonstrativa na Unity exercitando: eventos em loop contínuo, disparos múltiplos panoramizados em estéreo, camadas de tensão adaptativa, envelopes sustentados com `sustain`/`key_off` e parâmetro rotulado, snapshot `"DuckMusic"` de ducking, transição musical quantizada em compasso (`BAR`) entre estados de jogo e controle de mixagem via VCA.
- **AC:** Suite PlayMode executa o fluxo completo da demonstração e valida a ordem exata dos callbacks (disparos sequenciais, marcadores, liberação por `key_off`, transição musical).

---

### ÉPICO 5: OpenAudio Studio Desktop DAW em C++ / JUCE (`JP01` a `JP06`)

> **Nota Arquitetural:** A DAW de autoria é construída exclusivamente como uma aplicação desktop nativa em **C++ usando JUCE 8.0.12** (`studio/`), consumindo `oae_audio.dll` diretamente pela API C e invocando `oae_bankc.exe` em background. Toda a lógica de documento e histórico de Undo/Redo fica desacoplada dos widgets da UI para permitir testes headless (`--smoke-headless` e `--smoke-project`).

#### T16a (JP01) — Standalone Target & Native Integration Proof
- **Escopo:** Target CMake `oae_studio` com JUCE 8.0.12, empacotando `oae_audio.dll` e `oae_bankc.exe` ao lado do executável; diagnóstico claro se faltar DLL ou compilador; flag `--smoke-headless` que compila um projeto fixture, carrega o banco, renderiza áudio não-zero e encerra com código 0.
- **AC:** Executável compila em Debug e Release sem depender de Unity; `--smoke-headless` e `--smoke-device` passam.

#### T16b (JP02) — Projetos Portáteis & Publicação Assíncrona de Bancos
- **Escopo:** Criar, abrir e salvar atomicamente projetos `project.json` v1; importação de áudio copiando para dentro da pasta do projeto com caminhos relativos; detecção e relink de arquivos ausentes; compilação de bancos em thread de background a partir de um snapshot imutável do projeto com console de diagnósticos.
- **AC:** Mover a pasta inteira do projeto para um caminho com espaços e caracteres Unicode preserva a abertura e compilação; falhas de save ou build preservam os arquivos válidos anteriores.

#### T16c (JP03) — Modelo de Autoria, Navegador Hierárquico & Inspector Tipado
- **Escopo:**
  - Browser em árvore com pastas derivadas de `/` (`event:/`, `snapshot:/`, `bus:/`, `vca:/`), ícones de categoria, busca em tempo real com preservação de ancestrais, e operações de criar, renomear (`F2`), duplicar (`Ctrl+D`) e excluir (`Del`) com atualização automática de todas as referências cruzadas no projeto.
  - Inspector 100% tipado cobrindo todas as propriedades do schema v1 (sliders em dB/semitons/ms, seletores de bus/sample/evento, editor de playlists, condições, curvas de automação, moduladores RANDOM/AHDSR, cadeia de efeitos de bus, VCAs e snapshots).
  - Sistema de Undo/Redo por gesto (arrastar um slider gera apenas 1 passo de undo ao soltar o mouse; `Ctrl+Z` dentro de caixa de texto edita apenas o texto).
- **AC:** Teste automatizado cria evento de duas faixas com instrumento multi, parâmetro rotulado, condição, curva de automação e efeito de bus usando os controles tipados, renomeia itens mantendo integridade referencial, faz undo/redo e compila banco válido.

#### T16d (JP04) — Timeline, Waveforms & Edição Multi-Track de Clipes
- **Escopo:** Régua de tempo (segundos e batidas/compassos baseados em marcadores `tempo`), snap magnético configurável, zoom (`Ctrl+Wheel`), cabeçalhos de faixa com fader de volume e botões Mute (`M`) / Solo (`S`), geração assíncrona de envelopes de waveform Peak/RMS em background, arraste/redimensionamento de clipes, seleção múltipla e movimentação de clipes entre faixas, edição de marcadores/regiões na régua e edição de pontos de curva de automação na faixa.
- **AC:** Teste de projeto e smoke test verificam alinhamento de waveform com pitch/offset/loop, snap em segundos e batidas, Mute/Solo de faixas e persistência exata após salvar/reabrir.

#### T16e (JP05) — Audition Transport, Rebuild Debounced & Mixer Parity
- **Escopo:**
  - Barra de transporte (Play, Pause/Resume via stop+seek memorizado, Stop, Loop, Seek, KeyOff, timecode `mm:ss.ms`, seletores de parâmetros locais e globais).
  - Rastreamento de `ContentRevision`: edições estruturais marcam o preview como *out of date* e disparam rebuild assíncrono com debounce de 600 ms, trocando o banco em voo na message thread e restaurando posição/parâmetros/snapshots ativos.
  - Deck de Mixer com strips para Master, Buses e VCAs, faders em dB (`-80..+10`), regra de Solo hierárquica (muta irmãos fora do caminho solado, preservando ancestrais e descendentes), e medidores verticais Peak/RMS em tempo real.
- **AC:** Editar clipe durante reprodução reconstrói o preview automaticamente sem reiniciar o Studio e converge para a última revisão; fechar projeto durante build em voo não aplica banco órfão.

#### T16f (JP06) — Empacotamento Final & Fluxo Ponta-a-Ponta Studio  Jogo
- **Escopo:** Script `scripts/build_studio.ps1` gerando bundle completo com executável, DLL, `bankc` e projeto de exemplo; validação do banco exportado diretamente no projeto Unity.
- **AC:** Smoke tests `--smoke-headless`, `--smoke-project` e `--smoke-device` passam em Debug e Release; banco exportado pelo Studio carrega e toca no jogo consumidor.

---

### ÉPICO 6: Ponte Semântica & Automação por IA — MCP Server (`MCP-01` a `MCP-05`)

- **MCP-01 — Registro Semântico de Controles (`ComponentID`):** Atribuir IDs semânticos determinísticos (`setComponentID`) a todos os controles da DAW JUCE.
- **MCP-02 — Servidor MCP Local Embutido (JSON-RPC 2.0):** Loop de servidor MCP via `stdio` (`--mcp`) ou socket local `127.0.0.1:7451` despachando comandos de forma segura na message thread do JUCE.
- **MCP-03 — Catálogo de MCP Tools:** Implementar tools `studio_trigger_control`, `studio_select_item`, `studio_move_clip`, `studio_transport`, `studio_set_mixer_fader`, `studio_build_bank`.
- **MCP-04 — Catálogo de MCP Resources & JSON Patch Diffing:** Retornar diffs RFC 6902 (`nlohmann::json::diff`) do estado do projeto antes/depois de cada ação, além da telemetria acústica dos medidores.
- **MCP-05 — Suíte de Testes Headless com MCP (`--smoke-mcp`):** Runner automatizado que pilota a DAW inteiramente via JSON-RPC/MCP e valida os diffs e o áudio renderizado.

---

### ÉPICO 7: Monitoramento Acústico Avançado & Balística (`VU-01` a `VU-04`)

- **VU-01 — Seletor de Escalas no Mixer:** Suporte às escalas `dBFS Nominal`, Bob Katz `K-12`, `K-14`, `K-20` e `EBU R128 LUFS`.
- **VU-02 — Balística IEC 60268-17 & Peak Hold:** Ataque instantâneo, decaimento de 20dB/s, janela de integração RMS de 300ms e Peak Hold de 1.2s.
- **VU-03 — Processador ITU-R BS.1770 / EBU R128:** Filtro *K-Weighting* (high-pass em 100 Hz + high-shelf +4dB) + medição Momentânea (400ms), Short-Term (3s), Integrada com gating (-70LUFS absoluto e -10dB relativo) e True Peak (`dBTP` com oversampling 4 polifásico).
- **VU-04 — Testes de Calibração Acústica:** Senoide a -14dBFS marca exatamente `0.0 VU` na escala `K-14` (0.05dB).

---

### ÉPICO 8: Conectividade em Tempo Real / Live-Linking (`LIVE-01` a `LIVE-04`)

- **LIVE-01 — Servidor TCP Bridge (`:7450`):** Servidor assíncrono embutido no Studio com handshake de versão e validação de `bank_crc32`.
- **LIVE-02 — Módulo Cliente Unity (`LiveClient.cs`):** Conexão automática não-bloqueante em PlayMode / Development Builds.
- **LIVE-03 — Sincronização Bidirecional (<10ms):** Mover faders/parâmetros/curvas no Studio atualiza o jogo ao vivo; parâmetros alterados pelo gameplay na Unity movem os cursores no Studio.
- **LIVE-04 — Auditoria Remota & Fallback Offline:** Disparar eventos no Studio reproduz diretamente no `AudioListener` do jogo conectado, voltando para a engine local se o jogo desconectar.

---

### ÉPICO 9: Expansão de Áudio Interativo Avançado (`T18` a `T25`)

- **T18 — Audio Sends and Returns:** Envio pré/pós-fader ajustável de qualquer bus para buses de retorno (`return:/reverb`), com rejeição de ciclos em tempo de construção e remoção segura sem alocações na thread de áudio.
- **T19 — Per-Event Track Mixing and Effects:** Cada instância de evento passa a ter seu próprio sub-mixer de faixas com roteamento entre faixas, cadeia de efeitos por faixa e gerenciamento automático de cauda de efeito no stop.
- **T20 — Asynchronous Instruments:** Modo `async` em instrumentos (`SINGLE`/`MULTI`) permitindo que o som disparado continue tocando livremente mesmo quando o playhead da timeline salta em loops ou transições, com políticas de retrigger/overlap configuráveis.
- **T21 — Parameter Sheets and Region Triggering:** Abas de evento cujo eixo principal é um parâmetro (e não o tempo), disparando e liberando instrumentos conforme o cursor do parâmetro entra ou sai das regiões autoradas.
- **T22 — Programmer Instruments and Audio Tables:** Instrumentos cujo sample é resolvido dinamicamente em runtime pela game engine via chave de tabela de áudio (ideal para diálogos e localização multi-idioma), com callbacks AOT-safe fora da thread de áudio.
- **T23 — Unity Timeline Integration:** Faixas e clipes customizados para o `PlayableDirector` da Unity com scrubbing, preview em editor, automação de parâmetros e modos de parada no fim do clipe.
- **T24 — Unity Addressables and AssetBundles:** Providers assíncronos com contagem de referências para carregar pares `.bank` + `.catalog.json` via Addressables/AssetBundles.
- **T25 — Nested/Overlapping Loops & Event Pitch Timing:** Suporte a regiões de loop aninhadas/sobrepostas com prioridade explícita e modo de pitch de evento que acelera/desacelera a velocidade do playhead da timeline em conjunto com o pitch acústico.

---

### ÉPICO 10: Multiplataforma & Áudio Espacial 3D (`Milestone 2 & 3`)

- **M2-P01 — Port Linux x64:** Build `.so` com backend ALSA/PulseAudio no `miniaudio`.
- **M2-P02 — Port iOS / iPadOS:** Biblioteca estática (`.a`, `DllImport("__Internal")`), callbacks `MonoPInvokeCallback` 100% compatíveis com IL2CPP, tratamento de interrupções de sessão Core Audio.
- **M2-P03 — Port WebGL:** Compilação via Emscripten + backend Web Audio API adaptado aos limites de single-thread/AudioWorklet da Unity WebGL.
- **M2-ENC — Transcodificação Por Plataforma no `bankc`:** Codificação offline para Vorbis / ADPCM com controle de qualidade, taxa de amostragem e modo stream vs in-memory por plataforma alvo.
- **M2-PROF — Profiler em Tempo Real:** Telemetria de uso de CPU da thread de áudio, memória de samples por banco, contagem de vozes reais/virtuais e gráfico de instâncias ativas.
- **M3-3D — 3D Spatialization:** Posicionamento 3D (`position`, `velocity`, `forward`, `up` em coordenadas left-handed), atenuação por distância (`min/max distance`, curvas de rolloff linear/log/custom), efeito Doppler, múltiplos listeners e oclusão/obstrução por filtro passa-baixa.

---

## 9. Checklist de Débitos Técnicos Eliminados na Origem

Ao implementar no novo ambiente, garanta que os seguintes pontos (que eram débitos técnicos no protótipo anterior) já nasçam resolvidos:
1. **Lock-free System Lookup (ex-T02 R1):** Nunca use `std::mutex` para resolver `oae_system` dentro de `oae_system_render`. Use array fixo de ponteiros atômicos.
2. **Promoção Imediata após Reclaim Tardio (ex-T05 R5):** Em `update`, sempre execute a reciclagem de canais parados **antes** do ranking de virtualização de vozes, garantindo que o slot físico liberado seja promovido no mesmo frame.
3. **Transições Armadas por Posição (ex-T09 R5):** Um `TRANSITION_MARKER` que já foi ultrapassado pelo playhead quando sua condição era falsa **não** deve disparar no próximo ponto de quantização `BEAT`/`BAR` se a condição virar verdadeira depois; apenas `TRANSITION_REGION` permanece armada enquanto o playhead estiver dentro da região.
4. **Limites de Modulador `RANDOM` por Propriedade (ex-T10b-a R1):** Valide no `build()` limites específicos por propriedade para o modulador `RANDOM`: volume [0,80]dB, pitch [0,48]st, pan [0,1].
5. **Encadeamento Contínuo de Fades no `Channel::release` (ex-T10b-a R2):** Quando uma liberação agendada (`t_rel` no futuro) coexistir com um fade de canal já em andamento, avalie o ganho do fade no instante `t_rel` sem causar salto no valor atual.
6. **Cache de Miss em Automação de Efeito de Bus (ex-T10b-b R4):** Em `EffectAutomation`, se um bus não existir no momento, faça cache do estado "não encontrado" até que um novo bus seja criado (contador de época de buses), evitando string lookups repetidos em todo `update`.
7. **Limpeza Completa no `before_release` do Studio (ex-T10b-b R1):** Ao desregistrar um `ModelSet` ou liberar o `StudioSystem`, limpe tanto `m_sets` quanto `m_effect_autos`.
8. **Sincronização de Estado de Snapshots no Mixer da DAW (ex-T16d R3):** Ao reconstruir a UI do Mixer no Studio, inicialize os botões de toggle dos snapshots consultando `IsSnapshotActive` com `SetValueWithoutNotify`.

## 10. Invariantes Recuperadas e Pendências de Contrato

- `mute` é imediato e produz silêncio exato após a cadeia de efeitos pós-fader, inclusive caudas de reverb.
- Quando todos os ganhos do strip são unitários e pan é zero, não modificar as amostras: caminho identidade bit-exato.
- Na suavização dos filtros, o primeiro ajuste após criação ou saída de bypass aplica o alvo imediatamente; após convergência, parar recálculos desnecessários de seno/cosseno.
- A seção 3 explicita as equações antes corrompidas; os snippets não substituem validação de domínio, testes e contratos de erro.
- Aplicar o [contrato binário v1 aprovado](docs/specs/bank-v1-clarifications-proposal.md) à indexação `owner_index`, campos de `ParamRefRecord` e condições.
- Antes da T10b-b, esclarecer o valor de HOLD exatamente num ponto interno: o snippet atual seleciona o segmento anterior e retorna seu valor. Não alterar essa semântica por suposição.
- Antes da T13, concluir o contrato JSON e as políticas de recuperação/concor­rência de publicação descritas na seção 4.5.
- As referências antigas a T16d nos relatos de bugs são identificadores históricos; não alteram a divisão atual T16a–T16f.
