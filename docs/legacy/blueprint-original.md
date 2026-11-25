# OpenAudioEngine (OAE) — Master Blueprint, Architecture Spec & Autonomous Backlog

> **ESTADO ATUAL: SOMENTE DOCUMENTAÇÃO — IMPLEMENTAÇÃO NÃO AUTORIZADA.**
> Aguarde comando explícito do usuário antes de iniciar qualquer implementação, inclusive T01, scripts, dependências ou configuração de ferramentas. A sequência autônoma abaixo só entra em vigor após essa autorização.
> As instruções portáteis estão em [AGENTS.md](AGENTS.md), com regras de engenharia, economia de tokens, multiagente, memória persistente e seleção de modelo/esforço em [docs/ai/rules.md](docs/ai/rules.md).
> Toda funcionalidade deverá possuir documentação e exemplos de uso. Incertezas deverão ser esclarecidas com o usuário antes de executar o trabalho afetado; fora delas, atuar com autonomia no escopo autorizado.

> **INSTRUÇÃO PARA O AGENTE DE IA (MODO AUTÔNOMO):**
> Este documento contém a especificação completa, fórmulas matemáticas, contratos de API, formato binário, schema JSON, aprendizados de engenharia ("gotchas") e a sequência exata de tarefas (Backlog) para construir o middleware de áudio **OpenAudioEngine (`oae_audio`)** do zero.
> - Siga **estritamente** a ordem das tarefas (`T01` → `T17`, `JP01` → `JP06`, `MCP`, `VU`, `LIVE`, `T18` → `T25`).
> - **Nunca** viole as regras da thread de áudio em tempo real (Seção 2).
> - **Nunca** avance para a próxima tarefa sem que a build `Debug` e `Release` compilem com **zero warnings** e **100% dos testes unitários e de aceitação passem** no modo headless (`OAE_INIT_NO_DEVICE`).
> - Incorpore desde o primeiro código todas as **Lições Aprendidas e Prevenção de Bugs** listadas na Seção 5.

---

## 1. Visão Geral e Layout do Repositório

Um middleware completo de áudio interativo para jogos (equivalente open-source ao FMOD / Wwise), composto por:
1. **Core & Studio Runtime (`oae_audio.dll` / `.so` / `.a`):** Engine nativa em C++17/20 sobre `miniaudio`, exportando uma ABI C plana e estável (`oae_audio.h`).
2. **Bank Compiler CLI (`oae_bankc`):** Compilador de linha de comando que valida um `project.json` portátil e gera bancos binários `.bank` + catálogos `.bank.catalog.json`.
3. **Unity Integration Package (`com.openaudio.core`):** Camada P/Invoke blittable, wrappers seguros em C#, componentes de cena (`AudioManager`, `StudioEventEmitter`, `StudioBankLoader`, `StudioListener`) e seletor `[EventRef]`.
4. **OpenAudio Studio Desktop DAW (`studio/`):** Aplicação desktop standalone nativa (C++ / JUCE 8) para autoria visual de eventos, timeline multi-track, curvas de automação, mixer, snapshots e exportação de bancos.

### Estrutura de Diretórios
```text
CMakeLists.txt                 # Build raiz (C++17/20, MSVC /W4 ou -Wall -Wextra)
scripts/
  build.ps1                    # Script canônico de build e execução de testes nativos
  build_studio.ps1             # Script de build e smoke tests da DAW JUCE
  deploy_unity.ps1             # Copia a DLL Release para o pacote Unity
  unity_test.ps1               # Executa testes EditMode/PlayMode da Unity em batchmode
native/
  include/oae_audio.h          # Cabeçalho único da API C pública (ABI estável)
  src/
    api/                       # Wrappers extern "C" -> validação de parâmetros + catch-all -> repassa para core/studio
    core/                      # Core Engine (System, Sound, Channel, Bus, StripNode, VoiceMgmt, fx/)
    studio/                    # Studio Engine (EventModel, EventInstance, Timeline, Parameters, Automation, Snapshots, BankReader/Writer)
    common/                    # Utilitários (HandlePool geracional, SpscQueue, Ahdsr, Meter, Pcg32)
    miniaudio_impl.c           # Unidade de tradução única para miniaudio + stb_vorbis
  external/                    # Dependências vendorizadas (miniaudio, stb_vorbis, doctest, nlohmann_json)
  tools/bankc/                 # CLI Compiler (oae_bankc: project.json -> .bank + .bank.catalog.json)
  tests/                       # Testes automatizados doctest (*.cpp auto-globbed)
studio/                        # Aplicação Desktop DAW em C++ / JUCE 8.0.12
unity/OpenAudioUnity/          # Projeto host Unity + pacote UPM (Packages/com.openaudio.core/)
docs/                          # Especificações e tutoriais
```

---

## 2. Regras Inegociáveis de Tempo Real, Concorrência e Handles

### 2.1. Threading & Real-Time Safety (CRÍTICO)
1. **Single Game Thread:** A API pública é chamada a partir de uma única "game thread" (ou message thread na DAW). Não precisa de mutex entre múltiplas threads chamadoras, mas **DEVE ser 100% segura em relação à thread de áudio**.
2. **Audio Thread (Callback / Node Graph do `miniaudio`):**
   - **PROIBIDO:** Alocar ou liberar memória na heap (`new`, `delete`, `malloc`, `free`, `std::vector::push_back`), adquirir mutex bloqueante (`std::mutex`), fazer I/O de disco, fazer log, lançar exceções C++ ou chamar callbacks de código gerenciado (C#).
   - **Verificação em Testes:** Todo teste de renderização deve instrumentar um `HeapGuard` (contador atômico nos operadores globais `new`/`delete` na thread de renderização) e asserir `render_alloc_count == 0`.
3. **Comunicação Game Thread  Audio Thread:**
   - **Comandos estruturais:** Filas lock-free Single-Producer Single-Consumer (`SpscQueue<T>`).
   - **Parâmetros contínuos de Strip e Efeitos:** Mailboxes lock-free baseados em *seqlock* (contador atômico de sequência par/ímpar; o leitor da thread de áudio nunca faz spin-wait: se detectar leitura rasgada, mantém o registro anterior válido do bloco).
4. **Ciclo `oae_system_update()`:**
   - Chamado uma vez por frame pela game thread.
   - Ordem exata de execução dentro de `update(sys)`:
     1. **Studio `pre_update`:** avança parâmetros globais (seek speed)  avalia automações de efeitos de bus  atualiza instâncias de eventos (avança parâmetros locais, avalia curvas de automação, replaneja timelines se condições/`key_off` mudaram, agenda instrumentos dentro do *lookahead* de 200 ms, dispara comandos)  compõe alvos de Snapshots e VCAs nos buses.
     2. **Callback Dispatch:** drena a fila de callbacks gerada pelo Studio e invoca os handlers do usuário na game thread (com guarda de reentrância: chamar `update`, `render`, `release` ou descarregar banco dentro de um callback retorna `OAE_ERR_INVALID_STATE`).
     3. **Core Reclaim:** recicla canais que terminaram ou pararam (invalidando seus handles geracionais).
     4. **Voice Virtualization Ranking:** classifica todos os canais ativos por `(priority ASC, audibility DESC, creation_age ASC)`. Os melhores `max_physical_voices` tornam-se *reais* (processados no grafo); o restante torna-se *virtual* (mudo, sem custo de CPU, mas avançando relógio lógico).
     5. **Meter Window Swap:** fecha a janela de medição Peak/RMS de cada bus para leitura estável via `oae_bus_get_meter`.

### 2.2. Pool de Handles Geracionais (`HandlePool`)
- Todo handle opaco (`oae_system`, `oae_sound`, `oae_channel`, `oae_bus`, `oae_effect`, `oae_bank`, `oae_event_description`, `oae_event_instance`, `oae_vca`) é um `uint64_t`:
  - Bits `[0..31]`: Índice do slot (`uint32_t`).
  - Bits `[32..55]`: Geração (`uint24_t`, incrementada a cada liberação, nunca zero).
  - Bits `[56..63]`: Tag de tipo (`uint8_t`, impede passar um `oae_sound` onde se espera um `oae_bus`).
- O valor `0` é **sempre inválido** (exceto quando explicitamente documentado como alias para o Master Bus em `play_sound` e `bus_create`).
- Qualquer handle antigo (stale), de tipo trocado ou pertencente a outro `oae_system` deve retornar `OAE_ERR_INVALID_HANDLE` sem jamais causar crash.

### 2.3. Precedência Universal de Erros na API C
Toda função da API C valida argumentos nesta ordem estrita:
1. Ponteiros de saída `NULL` ou parâmetros fora de domínio de valor (`NaN`, `±Inf`, enum desconhecido, string malformada)  `OAE_ERR_INVALID_PARAM`.
2. Validação de handles (inválido, stale, ou de outro sistema)  `OAE_ERR_INVALID_HANDLE`.
3. Estado do sistema (ex: antes de `init`)  `OAE_ERR_NOT_INITIALIZED` ou `OAE_ERR_INVALID_STATE` (ex: dentro de callback).
4. Busca de caminho/nome inexistente  `OAE_ERR_NOT_FOUND` (ou `OAE_ERR_ALREADY_EXISTS` em duplicatas).
5. Falha de alocação  `OAE_ERR_OUT_OF_MEMORY`.
6. Barreira `try { ... } catch (...) { return OAE_ERR_INTERNAL; }` em todas as funções de `native/src/api/`. Nenhuma exceção C++ atravessa a fronteira da DLL.

---

## 3. Fórmulas Matemáticas e Algoritmos DSP Essenciais

Para que a implementação passe nos testes determinísticos de primeira, use exatamente estas equações:

### 3.1. Panning Estéreo (Constant-Power Law, Unity at Center)
Para `pan` [-1.0,+1.0]:

=(pan+1.0)4

GL=2(),GR=2()
- **Casos exatos obrigatórios (branch explícito ou snap no fim da rampa):**
  - Se `pan == 0.0f`: GL=1.0f,GR=1.0f (áudio sem pan é **bit-idêntico** à fonte).
  - Se `pan == -1.0f`: GL=2,GR=0.0f (canal direito tem silêncio exato `0.0f`).
  - Se `pan == +1.0f`: GL=0.0f,GR=2 (canal esquerdo tem silêncio exato `0.0f`).

### 3.2. De-Click de Pan e Volume de Bus
- Mudanças de `pan` (em canal ou bus) e mudanças de `volume` de bus **não dão salto abrupto**: fazem rampa linear ao longo de:
 
Ndeclick=16,round128sample_rate48000frames
- **Regra do primeiro valor:** O primeiro valor configurado logo após a criação do canal/bus aplica-se **instantaneamente** (sem rampa), garantindo que um som recém-iniciado já comece no ganho/pan correto desde a amostra 0.
- `mute` continua sendo uma chave imediata de silêncio exato (`0.0f`), aplicada **após** a cadeia de efeitos pós-fader (para silenciar inclusive caudas de reverb).

### 3.3. Curvas de Fade (`FadeSpec`)
Avaliadas na thread de áudio como função pura do `dsp_clock` (t[0,1]):
- `LINEAR`: g(t)=g0+(g1-g0)t
- `SCURVE` (Raised Cosine): g(t)=g0+(g1-g0)1-(t)2
- `LOG` (dB-linear com piso em -80dB=10-4):
  - Se g0==g1: retorne g1 imediatamente (evita emitir -80dB quando `0 -> 0`).
  - Caso contrário, faça clamp de a=(g0,10-4) e b=(g1,10-4), interpole g(t)=a(b/a)t, e faça snap exato para g1 quando t1.0.

### 3.4. Cadeia de Ganho do Strip de Canal (`StripNode`)
Cada canal possui um nó customizado no grafo `miniaudio` que multiplica, nesta ordem:

Gtotal(t)=GvolGfade(t)Genv_in(t)Genv_out(t)Gauto(t)Gahdsr_instr(t)Gahdsr_event(t)
- **Clippings de Borda de Bloco:** Antes de processar um chunk de até 256 frames, o `StripNode` descobre o próximo ponto de quina (fim do fade, fim do fade-in, início/fim do fade-out, fim da rampa de automação, ou qualquer uma das 5 quinas do AHDSR: `t_start`, `t_start + A`, `t_start + A + H`, `t_start + A + H + D`, `t_rel`, `t_rel + R`) e subdivide o bloco exatamente nas quinas. Assim, uma rampa linear por sub-bloco nunca atravessa uma quina do envelope!
- **Caminho Identidade Bit-Exato:** Quando todos os estágios estão em ganho unitário (`1.0f`) e `pan == 0.0f`, o loop não toca nas amostras (mantendo precisão bit-a-bit).

### 3.5. Suavização de Coeficientes de Filtros (`LOWPASS`, `HIGHPASS`, `EQ3`)
Mudar o cutoff de um filtro resonantemente (ex: Q=10, salto de 20kHz10Hz) causa explosão numérica (pico 1960) se os coeficientes Biquad mudarem de uma vez.
- **Solução:** Processar em sub-blocos de 32 frames. Manter estado `cur` e `target`.
- Constante de tempo =20ms: k=1--32sample_rate.
- A cada sub-bloco de 32 frames:
  - Frequências (`cutoff`, `low_xover`, `high_xover`) interpolam no **domínio logarítmico**: $\ln(f_{\text{cur}}) \mathrel{+}= k \cdot (\ln(f_{\text{tgt}}) - \ln(f_{\text{cur}}))$.
  - Ganhos do EQ3 interpolam em **dB**; Q interpola linearmente.
  - Quando a diferença relativa for <10-4, faça snap para `target` e pare de recalcular `sin`/`cos`.
  - O primeiro ajuste após criar ou tirar de bypass faz snap imediato.

### 3.6. Matemática do Beat Grid e Tempo Markers
- Para evitar drift de ponto flutuante em eventos longos, a posição em frames da batida n dentro de uma seção de tempo nunca é acumulada por soma iterativa; ela é calculada diretamente do índice n:
 
frame(n)=section_start_frame+llround(nframes_per_beat)
  onde frames_per_beat=sample_rate60.0bpm4.0ts_lower.

---

## 4. Especificações de Formatos (Binário `.bank` v1 e `project.json` v1)

### 4.1. Formato Binário `.bank` v1
- **Endianness:** Little-endian em todos os campos. Sem padding implícito.
- **Header:**
  - Magic: 4 bytes ASCII (`OAEB`, constante única).
  - `version`: `uint16_t` (`1`).
  - `flags`: `uint16_t` (`0`).
  - `chunk_count`: `uint32_t`.
- **Directory Table (`chunk_count` registros):**
  - `tag`: 4 bytes ASCII (`STRS`, `BUSS`, `EVTS`, `SMPL`, `DATA`, `LOGC`, `PARM`, `SNAP`, `VCAS`).
  - `offset`: `uint64_t` (absoluto no arquivo, **múltiplo de 16 bytes**, após o diretório, sem sobreposição).
  - `length`: `uint64_t`.
  - `crc32`: `uint32_t` (CRC-32/ISO-HDLC IEEE 802.3 calculado sobre os `length` bytes do payload).
- **Regra de Compatibilidade:** Chunks com tags desconhecidas têm seu enquadramento e CRC32 verificados e são ignorados (forward-compatible). Chunks ausentes opcionais (`LOGC`, `PARM`, `SNAP`, `VCAS`) deixam os campos correspondentes no padrão. Qualquer erro de magic, versão, truncamento, desalinhamento ou CRC32 retorna `OAE_ERR_FORMAT`.
- **Payloads dos Chunks:**
  1. `STRS`: `u32 count`, seguido de strings (`u32 byte_len` + bytes UTF-8 sem NUL). Demais chunks referenciam strings por índice `u32` na `STRS`.
  2. `BUSS`: `u32 count` de buses (`u32 name_str`, `i32 parent_idx` [-1=master], `f32 linear_vol`, lista de efeitos: `i32 pos`, `i32 type`, `u8 bypass`, `i32 sidechain_bus_idx`, `u32 param_count` + `f32 params[]`, `f32 wet`, `f32 dry`).
  3. `EVTS`: `u32 count` de eventos (`u64 guid_hi, u64 guid_lo`, `u32 path_str`, `u32 bus_str`, `f32 vol_db, f32 pitch_st, f32 pan`, `u32 max_instances`, `u8 steal_mode`, `u32 cooldown_ms`, `i32 priority`, `u32 stop_fade_ms`, `u32 track_count` + tracks, `u32 user_prop_count` + pares `[key_str, val_str]`).
     - Cada Track: `Guid`, `f32 vol_db`, `f32 pan`, `u32 instr_count` + instrumentos.
     - Cada Instrumento (registro fixo com todos os campos variantes presentes): `Guid`, `u8 kind`, `f64 offset_ms, f64 length_ms, f64 start_offset_ms`, `u8 loop`, `f64 fade_in_ms, f64 fade_out_ms`, `f32 vol_base, f32 vol_spread, f32 pitch_base, f32 pitch_spread`, `i32 sample_idx`, `Guid event_ref`, `u8 cmd_type`, `Guid cmd_target`, `u8 playlist_mode`, `u8 playlist_scope`, `u32 entry_count` + entries (`i32 sample_idx`, `Guid event_ref`, `f32 weight`).
  4. `SMPL`: `u32 count` de metadados de sample (`u64 data_rel_offset` [múltiplo de 16], `u64 byte_length`, `u64 decoded_frames`, `u32 sample_rate`).
  5. `DATA`: Bytes codificados originais (WAV/OGG/MP3/FLAC) concatenados com zero-padding até alinhamento de 16 bytes.
  6. `LOGC`: `u32 count` (1 por entrada em `EVTS`, na mesma ordem, com `Guid` correspondente)  lista de `LogicItem` (`u8 kind`, `Guid`, `f64 pos_ms, f64 end_ms`, `u32 name_str`, `f64 bpm`, `i32 ts_num, i32 ts_den, i32 repeat_count`, `u8 until_key_off`, `u32 dest_str`, `u8 quant`, `f64 xfade_ms`, `i32 cond_idx`).
  7. `PARM`: Lista de parâmetros globais + lista por evento (`Guid`, parâmetros locais, condições, moduladores, automações, e lista de `i32 condition_index` na ordem achatada dos instrumentos) + lista de `EffectAutomation`.
  8. `SNAP`: 1 registro por `EVTS` (`Guid`, `u8 event_kind`, `u8 snap_mode`, `u32 fade_in_ms`, `i32 intensity_param_idx`, lista de alvos: `u32 bus_str`, `f32 value`, `i32 param_id`, `i32 pos`, `u32 index`, `i32 effect_type`).
  9. `VCAS`: Lista de VCAs (`u32 path_str`, lista de `u32 bus_str`, lista de `u32 event_str`).

### 4.2. Schema JSON de Projeto (`project.json` v1) e Catálogo (`.bank.catalog.json`)
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
- **Publicação Atômica do `bankc`:**
  - Grava os arquivos em diretório temporário `.bankc-stage` ao lado do destino.
  - Substitui atomicamente o `.bank.catalog.json` e depois o `.bank` (fazendo rollback do catálogo se o rename do `.bank` falhar).
  - O `.bank.catalog.json` contém `{ "version": 1, "bank_size": <bytes>, "bank_crc32": <u32>, "events": [{ "path": "event:/...", "kind": "event" }] }`.

---

## 5. O "Ouro": Aprendizados, Armadilhas e Bugs Já Resolvidos ("Gotchas")

> **ATENÇÃO IA IMPLEMENTADORA:** Estes erros custaram centenas de milhares de tokens para diagnosticar no protótipo original. **Implemente as soluções abaixo diretamente no primeiro código de cada tarefa:**

1. **Bug de Duplicação de Instrumentos em Loops (T09 Bug 1 & 2):**
   - *Armadilha:* Comparar `InstrRun.last_seg == seg.id` para saber se um instrumento já foi agendado no segmento faz com que, quando o plano contém dois segmentos no horizonte com o mesmo instrumento (ex: um loop curto), o instrumento seja re-instanciado a cada `update` (causando multiplicação exponencial de vozes). Além disso, zerar `last_seg = 0` ao cancelar canais num `replan()` faz o segmento mantido agendar um segundo canal duplicado por cima do que já estava tocando.
   - *Solução obrigatória:* Os IDs de segmento crescem monotonicamente ao longo do plano. Use `last_seg >= seg.id` para pular instrumentos já agendados. Em `replan()`, faça clamp de todo `last_seg` para o maior `seg.id` mantido (`last_seg = std::min(last_seg, kept_seg_id)`), e `cancel_channel` não deve zerar `last_seg`.

2. **Bug de Disparo Retroativo no Re-walk do Horizonte (T09 Bug 3):**
   - *Armadilha:* Ao estender o plano de timeline (`extend_plan`), re-escanear o último segmento aberto (`HORIZON`) desde o início dele (`tl_start`) faz com que uma `transition_region` cujo ponto de quantização ficou no passado dispare retroativamente no `dsp_clock` atual.
   - *Solução obrigatória:* Ao estender o segmento `HORIZON`, passe `min_tl = old_tl_end` (tudo antes do horizonte antigo já foi decidido em updates anteriores).

3. **Empate de Ordem de Callbacks no Mesmo Frame DSP (T09 Bug 4):**
   - *Armadilha:* Gerar callbacks de `MARKER`/`BEAT` com limite semiaberto `[last, now)` enquanto `SOUND_PLAYED` usa `<= now` faz com que um som e uma batida no exato mesmo frame sejam entregues em updates diferentes, quebrando a ordem determinística (`STARTED`, `MARKER`, `BEAT`, `SOUND_STOPPED`, `SOUND_PLAYED`, `STOLEN`, `STOPPED`).
   - *Solução obrigatória:* Gere `MARKER` e `BEAT` com limite `now + 1` (inclusivo de `now`).

4. **Burst Audível de Curva de Timeline após Salto de Loop (Tech Debt T10b-b R6):**
   - *Armadilha:* Como instrumentos são agendados com até `200 ms` de *lookahead* antes de soarem, se você avaliar uma curva de automação com `source == TIMELINE` usando a posição do playhead *no instante do agendamento*, um instrumento que entra logo após um salto de loop começará com o ganho do fim do loop (ex: `0 dB` em vez de `-30 dB`) durante 1 frame de update.
   - *Solução obrigatória:* Para canais agendados no futuro via lookahead, avalie o valor inicial das curvas `TIMELINE` na **posição de timeline em que o canal efetivamente começará a tocar** (`seg.tl_start + (chan_start_dsp - seg.dsp_start)`), e não no `playhead_at(now)`.

5. **Churn de Envelope de Fade-Out durante Sweep de Pitch (T09 N1 / T10b-b R2):**
   - *Armadilha:* Quando o pitch de um one-shot muda, o fim natural da amostra (`nat_end`) muda. Se você reposicionar o envelope de fade-out a cada micro-variação de pitch e reiniciar o fade a partir de `from = 1.0f`, o áudio dará estalos (clicks) se o fade-out já estiver em andamento.
   - *Solução obrigatória:* (a) Só chame `reapply_ends()` se `nat_end` se deslocou mais de `512 frames` (histerese de 1 bloco). (b) Se o envelope de fade-out já tiver começado no `dsp_clock` atual, continue a rampa a partir do **valor atual do envelope**, nunca de `1.0f`.

6. **Mutex no Caminho de Renderização (Tech Debt T02 R1):**
   - *Armadilha:* Usar um `std::mutex` global em `get_system(handle)` chamado dentro de `system_render()`.
   - *Solução obrigatória:* Na thread de renderização, resolva o ponteiro do sistema via tabela estática de slots atômicos (`std::atomic<System*>` + geração atômica) para que `render()` seja **100% lock-free**.

7. **Reamostragem de Sons em Memória (`create_from_memory`) no `miniaudio` (T05 Fix):**
   - *Armadilha:* Ao decodificar um WAV/OGG em memória com `ma_decoder_init_memory`, se você configurar o decoder para a taxa nativa do arquivo sem passar `config.sampleRate = engine_sample_rate` (ou sem ativar reamostragem no `ma_audio_buffer_ref`), um arquivo de `44100 Hz` tocado num engine de `48000 Hz` tocará no pitch e duração errados.
   - *Solução obrigatória:* Sempre inicialize o `ma_decoder` de `create_from_memory` com `channels = engine_channels`, `format = ma_format_f32` e `sampleRate = engine_sample_rate` (ou armazene a taxa nativa e deixe o `ma_sound` reamostrar corretamente).

8. **Proteção contra `NaN` / `Inf` e Overflow no Medidor e Sidechain (T07 R9 & R10):**
   - *Armadilha:* Um único sample gigante (51012) ou `NaN` vindo de um filtro instável ou sidechain corrompe o acumulador de soma de quadrados do RMS ou trava o compressor.
   - *Solução obrigatória:* No `Meter` e no `SidechainTap`, trate qualquer amostra não-finita (`!std::isfinite(s)`) como `0.0f` (silêncio) e faça clamp da magnitude máxima medida em `1e9f`.

9. **Armadilha do PowerShell 5.1 com `vcvarsall.bat` e `2>&1`:**
   - *Armadilha:* No Windows PowerShell 5.1, redirecionar `2>&1` ao invocar `cmd /c "vcvars64.bat && cmake ..."` transforma qualquer escrita em `stderr` (como avisos normais do CMake/Ninja) em `ErrorRecord` terminante se `$ErrorActionPreference = 'Stop'`.
   - *Solução obrigatória:* Em `scripts/build.ps1`, redirecione a saída dentro do próprio `cmd.exe` para um arquivo de log ou execute sem `2>&1` no PowerShell, verificando `$LASTEXITCODE`.

10. **Ciclo de Vida de Handles na Reconstrução de Preview da DAW JUCE (JP05 / T16d R7 & R8):**
    - *Armadilha:* Descarregar o banco de preview antigo *antes* de compilar o novo deixa a DAW muda se a compilação falhar; e esquecer de parar a audição ao trocar de projeto mantém o evento antigo tocando.
    - *Solução obrigatória:* Compile o snapshot em background. Só quando o arquivo `.bank` novo estiver pronto e validado, na message thread: (1) salve posição/parâmetros/snapshots ativos, (2) pare e dê `release` nas instâncias ativas, (3) descarregue o banco antigo, (4) carregue o banco novo e (5) readquira os handles e restaure o estado. Ao abrir/criar outro projeto, pare imediatamente a audição e limpe o mapa de parâmetros.

---

## 6. Backlog Mestre de Épicos e Tarefas (Sequência de Execução Autônoma)

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
  - `oae_system_play_sound(sys, sound, bus, paused, &ch)`; controles de canal: `stop`, `set_paused/get_paused`, `set_volume/get_volume`, `set_pitch/get_pitch`, `is_playing`, `get_position_ms/set_position_ms`.
  - Suporte a WAV, MP3, FLAC, OGG Vorbis. Canais one-shot finalizados são reciclados em `update` (invalidando o handle).
- **AC:** Todos os 4 formatos decodificam; senoide tocada renderiza com RMS esperado $\pm 5\%$; volume `0.5` reduz RMS pela metade $\pm 5\%$; pausado  silêncio; pitch `2.0` reduz duração pela metade; handle de canal após término  `INVALID_HANDLE`; `NaN` ou negativo em volume/pitch  `INVALID_PARAM`.

#### T04 — Bus / mixer graph
- **Escopo:**
  - `oae_system_get_master_bus`, `oae_bus_create(sys, name, parent, &bus)`, `oae_system_get_bus(sys, "bus:/sfx/ui", &bus)`, `set_volume/get_volume`, `set_mute/get_mute`, `set_paused/get_paused`, `stop_all`, `release`.
  - Hierarquia sobre `ma_sound_group` + `StripNode` customizado. Profundidade máxima de 32 níveis abaixo do master (`> 32`  `INVALID_PARAM`). `bus_release` destrói recursivamente a subárvore e seus canais.
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
- **AC:** Pan `-1`  canal R silêncio exato; pan `0`  L/R idênticos; potência constante em 0.5 (0.5dB); fade linear de 500 ms tem RMS esperado no meio $\pm 10\%$; `stop_with_fade` termina em silêncio e invalida o handle no próximo `update`; dois canais agendados com 120 ms (5760 frames a 48 kHz) de diferença iniciam com precisão amostral.

#### T07 — Bus effects (DSP chain) & metering
- **Escopo:**
  - Cadeia de até 8 efeitos pré-fader e 8 pós-fader por bus: `oae_bus_add_effect`, `oae_bus_add_effect_ex`, `oae_effect_set_param/get_param`, `set_bypass/get_bypass`, `set_wet_dry/get_wet_dry`, `set_sidechain`, `release`.
  - Efeitos: `LIMITER` (look-ahead 1.5 ms, brickwall), `COMPRESSOR` (com sidechain tap de 1 bloco de outro bus), `EQ3` (Linkwitz-Riley 4ª ordem), `LOWPASS`, `HIGHPASS` (Biquad 12 dB/oct com suavização de 20 ms), `REVERB` (Freeverb), `DELAY` (até 2s pré-alocado).
  - Medidor pós-fader/pós-mute: `oae_bus_get_meter(bus, &meter)` com Peak e RMS por canal.
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
  - Implementar serializador (`bank_writer.*`) e parser (`bank_reader.*`) para todos os 9 chunks da Seção 4.1 (`STRS`, `BUSS`, `EVTS`, `SMPL`, `DATA`, `LOGC`, `PARM`, `SNAP`, `VCAS`).
  - API C: `oae_studio_load_bank_memory`, `oae_studio_load_bank_file`, `oae_studio_unload_bank`.
- **AC:** Round-trip `write -> read` preserva igualdade exata de todo o modelo T08–T11; 10.000 mutações aleatórias de bytes (fuzzing) nunca causam crash e retornam `OAE_ERR_FORMAT`; descarregar banco com instâncias tocando para todas as instâncias e invalida seus handles.

#### T13 — Bank compiler CLI (`oae_bankc`)
- **Escopo:**
  - Executável `native/tools/bankc/oae_bankc.exe <project.json> -o <out.bank>`.
  - Faz parse do schema JSON v1 (Seção 4.2), resolve caminhos de áudio relativos ao JSON, valida referências cruzadas e ciclos, e publica atomicamente `<out.bank>` e `<out.bank.catalog.json>`.
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

#### JP01 — Standalone Target & Native Integration Proof
- **Escopo:** Target CMake `oae_studio` com JUCE 8.0.12, empacotando `oae_audio.dll` e `oae_bankc.exe` ao lado do executável; diagnóstico claro se faltar DLL ou compilador; flag `--smoke-headless` que compila um projeto fixture, carrega o banco, renderiza áudio não-zero e encerra com código 0.
- **AC:** Executável compila em Debug e Release sem depender de Unity; `--smoke-headless` e `--smoke-device` passam.

#### JP02 — Projetos Portáteis & Publicação Assíncrona de Bancos
- **Escopo:** Criar, abrir e salvar atomicamente projetos `project.json` v1; importação de áudio copiando para dentro da pasta do projeto com caminhos relativos; detecção e relink de arquivos ausentes; compilação de bancos em thread de background a partir de um snapshot imutável do projeto com console de diagnósticos.
- **AC:** Mover a pasta inteira do projeto para um caminho com espaços e caracteres Unicode preserva a abertura e compilação; falhas de save ou build preservam os arquivos válidos anteriores.

#### JP03 — Modelo de Autoria, Navegador Hierárquico & Inspector Tipado
- **Escopo:**
  - Browser em árvore com pastas derivadas de `/` (`event:/`, `snapshot:/`, `bus:/`, `vca:/`), ícones de categoria, busca em tempo real com preservação de ancestrais, e operações de criar, renomear (`F2`), duplicar (`Ctrl+D`) e excluir (`Del`) com atualização automática de todas as referências cruzadas no projeto.
  - Inspector 100% tipado cobrindo todas as propriedades do schema v1 (sliders em dB/semitons/ms, seletores de bus/sample/evento, editor de playlists, condições, curvas de automação, moduladores RANDOM/AHDSR, cadeia de efeitos de bus, VCAs e snapshots).
  - Sistema de Undo/Redo por gesto (arrastar um slider gera apenas 1 passo de undo ao soltar o mouse; `Ctrl+Z` dentro de caixa de texto edita apenas o texto).
- **AC:** Teste automatizado cria evento de duas faixas com instrumento multi, parâmetro rotulado, condição, curva de automação e efeito de bus usando os controles tipados, renomeia itens mantendo integridade referencial, faz undo/redo e compila banco válido.

#### JP04 — Timeline, Waveforms & Edição Multi-Track de Clipes
- **Escopo:** Régua de tempo (segundos e batidas/compassos baseados em marcadores `tempo`), snap magnético configurável, zoom (`Ctrl+Wheel`), cabeçalhos de faixa com fader de volume e botões Mute (`M`) / Solo (`S`), geração assíncrona de envelopes de waveform Peak/RMS em background, arraste/redimensionamento de clipes, seleção múltipla e movimentação de clipes entre faixas, edição de marcadores/regiões na régua e edição de pontos de curva de automação na faixa.
- **AC:** Teste de projeto e smoke test verificam alinhamento de waveform com pitch/offset/loop, snap em segundos e batidas, Mute/Solo de faixas e persistência exata após salvar/reabrir.

#### JP05 — Audition Transport, Rebuild Debounced & Mixer Parity
- **Escopo:**
  - Barra de transporte (Play, Pause/Resume via stop+seek memorizado, Stop, Loop, Seek, KeyOff, timecode `mm:ss.ms`, seletores de parâmetros locais e globais).
  - Rastreamento de `ContentRevision`: edições estruturais marcam o preview como *out of date* e disparam rebuild assíncrono com debounce de 600 ms, trocando o banco em voo na message thread e restaurando posição/parâmetros/snapshots ativos.
  - Deck de Mixer com strips para Master, Buses e VCAs, faders em dB (`-80..+10`), regra de Solo hierárquica (muta irmãos fora do caminho solado, preservando ancestrais e descendentes), e medidores verticais Peak/RMS em tempo real.
- **AC:** Editar clipe durante reprodução reconstrói o preview automaticamente sem reiniciar o Studio e converge para a última revisão; fechar projeto durante build em voo não aplica banco órfão.

#### JP06 — Empacotamento Final & Fluxo Ponta-a-Ponta Studio  Jogo
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

## 7. Checklist de Débitos Técnicos Eliminados na Origem

Ao implementar no novo ambiente, garanta que os seguintes pontos (que eram débitos técnicos no protótipo anterior) já nasçam resolvidos:
1. **Lock-free System Lookup (ex-T02 R1):** Nunca use `std::mutex` para resolver `oae_system` dentro de `oae_system_render`. Use array fixo de ponteiros atômicos.
2. **Promoção Imediata após Reclaim Tardio (ex-T05 R5):** Em `update`, sempre execute a reciclagem de canais parados **antes** do ranking de virtualização de vozes, garantindo que o slot físico liberado seja promovido no mesmo frame.
3. **Transições Armadas por Posição (ex-T09 R5):** Um `TRANSITION_MARKER` que já foi ultrapassado pelo playhead quando sua condição era falsa **não** deve disparar no próximo ponto de quantização `BEAT`/`BAR` se a condição virar verdadeira depois; apenas `TRANSITION_REGION` permanece armada enquanto o playhead estiver dentro da região.
4. **Limites de Modulador `RANDOM` por Propriedade (ex-T10b-a R1):** Valide no `build()` limites específicos por propriedade para o modulador `RANDOM`: volume [0,80]dB, pitch [0,48]st, pan [0,1].
5. **Encadeamento Contínuo de Fades no `Channel::release` (ex-T10b-a R2):** Quando uma liberação agendada (`t_rel` no futuro) coexistir com um fade de canal já em andamento, avalie o ganho do fade no instante `t_rel` sem causar salto no valor atual.
6. **Cache de Miss em Automação de Efeito de Bus (ex-T10b-b R4):** Em `EffectAutomation`, se um bus não existir no momento, faça cache do estado "não encontrado" até que um novo bus seja criado (contador de época de buses), evitando string lookups repetidos em todo `update`.
7. **Limpeza Completa no `before_release` do Studio (ex-T10b-b R1):** Ao desregistrar um `ModelSet` ou liberar o `StudioSystem`, limpe tanto `m_sets` quanto `m_effect_autos`.
8. **Sincronização de Estado de Snapshots no Mixer da DAW (ex-T16d R3):** Ao reconstruir a UI do Mixer no Studio, inicialize os botões de toggle dos snapshots consultando `IsSnapshotActive` com `SetValueWithoutNotify`.

