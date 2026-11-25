# Contrato T16d (JP04) — Timeline, Waveforms, Ruler Logic & Clip Editing

## 1. Visão Geral
Este documento especifica a arquitetura, os contratos de dados e os critérios de validação da tarefa **T16d (JP04)** do OpenAudio Studio (DAW desktop nativa em C++17 com JUCE 8.0.12).

O escopo compreende:
1. **Régua de Tempo & Marcadores de Lógica (`TimelineRuler`)**:
   - Graduação de tempo em segundos / milissegundos e compassos / batidas musicais calculados a partir de marcadores `tempo` (`bpm`, `ts_upper`, `ts_lower`, `position_ms`).
   - Visualização e manipulação interativa de elementos de lógica: marcadores nomeados (`marker`), alterações de andamento (`tempo`), regiões de repetição (`loop_region`), marcadores/regiões de transição (`transition_marker`, `transition_region`) e pontos de sustentação (`sustain_point`).
   - Cursor de reprodução / playhead com scrubbing bidirecional.
2. **Grade de Snap Magnético (`TimelineSnap`)**:
   - Modos de alinhamento configuráveis: Off / Livre, Divisões de Tempo (1s, 500ms, 100ms, 50ms, 10ms), Compassos inteiros (Bar) e Batidas musicais (1/1, 1/2, 1/4, 1/8, 1/16 de batida).
   - Cálculo determinístico de snap para qualquer posição no domínio contínuo da timeline [0, 86.400.000 ms].
3. **Decodificação Assíncrona de Waveforms & Cache de Envelopes (`WaveformCache`)**:
   - Carregamento e decodificação em thread de segundo plano para arquivos de áudio WAV/OGG/FLAC/MP3 em `assets/`.
   - Extração de envelopes com multi-resolução para picos mínimo/máximo e RMS, sem bloqueio da UI (`paint()`).
   - Transformação gráfica determinística do envelope sob `start_offset_ms` (corte inicial do sample), `pitch_semitones` (escala temporal de reprodução `2^(semitones/12)`), `loop` (repetição cíclica em mosaico quando `length_ms > duration_ms`) e rampas de atenuação de `fade_in_ms` e `fade_out_ms`.
4. **Cabeçalhos de Faixa & Controles de Mixagem (`TrackHeaderPanel`)**:
   - Fader de volume em dB (`-80.0 .. +10.0 dB`, com ganho 0 para `<= -80 dB`) e slider de pan (`-1.0 .. +1.0`).
   - Botões de alternância Mute (`M`) e Solo (`S`) por faixa, com suporte a solo hierárquico e persistência transacional via `user_props` em conformidade estrita com o schema `project.json` v1.
   - Alternância de exibição de sub-faixas de automação.
5. **Edição Interativa Multi-Track de Clipes (`TimelineTrackLane` / `ClipComponent`)**:
   - Blocos de clipe com identificação por cor baseada no tipo de instrumento (Single, Multi, Event, Command, Silence).
   - Movimentação horizontal com snap magnético.
   - Movimentação vertical entre faixas do mesmo evento.
   - Redimensionamento de bordas (início e término).
   - Manipuladores de fade triangular/curvo nos cantos superiores (fade-in e fade-out).
   - Seleção individual e múltipla (retângulo de seleção / Shift-Click) para movimentação coordenada.
6. **Edição de Curvas de Automação de Faixa (`AutomationLaneComponent`)**:
   - Visualização de curvas de automação para volume, pan ou parâmetros vinculados.
   - Adição, movimentação, clamping e exclusão de pontos garantindo monotonicidade estrita no eixo X.
7. **Integração com o Modelo de Autoria & Undo/Redo**:
   - Toda alteração física (arraste de clipe, alteração de fade, fader, mute/solo, pontos de curva) executa dentro de gestos atômicos (`begin_gesture()`, `end_gesture()`), integrando-se perfeitamente com `Ctrl+Z` (Undo) e `Ctrl+Y` (Redo).

---

## 2. Contratos de Dados & Snap Magnético

### 2.1 Modos de Snap
A classe `TimelineSnap` define os seguintes modos:
```cpp
enum class SnapMode {
    None,
    Seconds_1s,
    Seconds_500ms,
    Seconds_100ms,
    Seconds_50ms,
    Seconds_10ms,
    Bar,
    Beat,
    Beat_Half,
    Beat_Quarter,
    Beat_Eighth,
    Beat_Sixteenth
};
```

### 2.2 Algoritmo de Snap por Compasso e Batida
Para uma posição de tempo $t$ em milissegundos e um conjunto de marcadores de andamento ordenados por posição:
1. Encontra-se o marcador `tempo` ativo $M_k$ mais próximo tal que $pos_k \le t$. Se nenhum marcador anterior existir, utiliza-se o andamento padrão (120 BPM, 4/4 na posição 0 ms).
2. Duração da semínima (batida de referência em ms):
   $$\Delta_{beat} = \frac{60000}{\text{bpm}}$$
3. Duração do compasso (bar em ms):
   $$\Delta_{bar} = \Delta_{beat} \times \left(\text{ts\_upper} \times \frac{4}{\text{ts\_lower}}\right)$$
4. A fração de subdivisão $S$ para cada modo:
   - `Bar`: $S = \Delta_{bar}$
   - `Beat`: $S = \Delta_{beat}$
   - `Beat_Half`: $S = \frac{\Delta_{beat}}{2}$
   - `Beat_Quarter`: $S = \frac{\Delta_{beat}}{4}$
   - `Beat_Eighth`: $S = \frac{\Delta_{beat}}{8}$
   - `Beat_Sixteenth`: $S = \frac{\Delta_{beat}}{16}$
5. A posição magnetizada é dada por:
   $$t_{snapped} = \max\left(0.0, pos_k + \operatorname{round}\left(\frac{t - pos_k}{S}\right) \times S\right)$$

---

## 3. Waveform Cache & Alinhamento Temporal

### 3.1 Estrutura do Envelope (`WaveformEnvelope`)
```cpp
struct WaveformEnvelope {
    double duration_ms{ 0.0 };
    uint32_t sample_rate{ 48000 };
    uint32_t channels{ 2 };
    double points_per_second{ 200.0 };
    std::vector<float> min_peaks;
    std::vector<float> max_peaks;
    std::vector<float> rms_levels;
};
```

### 3.2 Transformações de Alinhamento na Renderização
Dado um clipe de instrumento com:
- `offset_ms`: Posição inicial na timeline da faixa.
- `length_ms`: Extensão visual do clipe na timeline.
- `start_offset_ms`: Ponto de início de reprodução dentro do arquivo de áudio.
- `pitch_semitones`: Afinação relativa.
- `loop`: Se o áudio se repete continuamente até o fim de `length_ms`.
- `fade_in_ms`, `fade_out_ms`: Duração dos fades de entrada e saída.

A taxa de velocidade de reprodução é:
$$R_{pitch} = 2.0^{\frac{\text{pitch\_semitones}}{12.0}}$$

Para um ponto na timeline $t \in [\text{offset\_ms}, \text{offset\_ms} + \text{length\_ms}]$:
1. Tempo decorrido no clipe: $\Delta t = (t - \text{offset\_ms}) \times R_{pitch}$
2. Tempo de áudio no arquivo de origem:
   - Se `loop == false`:
     $$t_{audio} = \min(\text{start\_offset\_ms} + \Delta t, \text{duration\_ms})$$
   - Se `loop == true`:
     $$t_{audio} = \operatorname{fmod}(\text{start\_offset\_ms} + \Delta t, \text{duration\_ms})$$
3. Índice do envelope no cache:
   $$idx = \operatorname{clamp}\left(\operatorname{floor}\left(\frac{t_{audio}}{1000.0} \times \text{points\_per\_second}\right), 0, N - 1\right)$$
4. Ganho de fade linear:
   - Fade In ($t < \text{offset\_ms} + \text{fade\_in\_ms}$):
     $$G_{in} = \frac{t - \text{offset\_ms}}{\text{fade\_in\_ms}}$$
   - Fade Out ($t > \text{offset\_ms} + \text{length\_ms} - \text{fade\_out\_ms}$):
     $$G_{out} = \frac{\text{offset\_ms} + \text{length\_ms} - t}{\text{fade\_out\_ms}}$$
   - Ganho final aplicado à altura visual da onda: $G_{fade} = \min(G_{in}, G_{out}, 1.0)$.

---

## 4. Persistência de Mute e Solo de Faixas
Em estrita conformidade com o schema `project.json` v1 (`project-v1.schema.proposed.json`), onde `track` não permite campos extras desconhecidos sob `additionalProperties: false`, o estado de Mute e Solo das faixas do Studio é persistido no dicionário `user_props` do próprio evento:
```json
"user_props": {
    "track_0_mute": "true",
    "track_1_solo": "false"
}
```
Isso garante:
1. Validação estrita aprovada pelo `oae_bankc` com 0 erros de schema.
2. Persistência 100% confiável entre salvar e recarregar projetos.
3. Transições reversíveis no histórico de Undo/Redo do `AuthoringModel`.

---

## 5. Critérios de Aceitação (AC)

1. **Snap Magnético**: O cálculo de snap retorna valores matematicamente exatos tanto para divisões temporais em segundos quanto para compassos e batidas sob múltiplos marcadores de andamento.
2. **Geração e Alinhamento de Waveform**: O `WaveformCache` decodifica arquivos WAV reais sem bloquear a thread principal, gerando envelopes Peak e RMS que respeitam o `start_offset_ms`, a escala de pitch e a repetição por loop.
3. **Edição Multi-Track**: O `AuthoringModel` suporta mover clipes na timeline, transferir clipes entre faixas, redimensionar início/fim e ajustar fades com suporte integral a Undo/Redo.
4. **Mute e Solo**: As propriedades Mute e Solo por faixa operam de forma independente e sobrevivem ao ciclo completo de salvamento no disco (`document.save()`) e reabertura (`document.load()`).
5. **Automação e Smoke Test**: O executável `oae_studio.exe --smoke-timeline` executa de forma autônoma e aprova todas as etapas acima com exit code 0 em Debug e Release.
