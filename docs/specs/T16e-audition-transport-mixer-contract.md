# Contrato T16e (JP05) — Audition Transport, Debounced Preview Rebuild & Mixer Parity

## 1. Visão Geral
Este documento especifica a arquitetura técnica, os contratos de execução e os critérios de validação da tarefa **T16e (JP05)** do OpenAudio Studio (DAW desktop nativa em C++17 com JUCE 8.0.12).

O escopo compreende:
1. **Barra de Transporte & Controles de Audição (`AuditionTransport` / `TransportBar`)**:
   - Controle completo de reprodução: Play, Pause / Resume (via memorização de tempo e parada/retomada imediata), Stop (com fade-out ou parada imediata), Loop de timeline, Seek para qualquer milissegundo, e KeyOff para instrumentos sustentados e regiões de repetição.
   - Indicador de tempo decorrido no formato timecode `mm:ss.ms` atualizado a 30 Hz.
   - Sliders e seletores interativos para todos os parâmetros locais do evento selecionado e parâmetros globais do projeto.
2. **Reconstrução Automática Debounced de Preview (`PreviewRebuilder`)**:
   - Rastreamento estrito de `ContentRevision`: qualquer alteração no modelo incrementa o contador atômico de revisão.
   - Debounce configurado para 600 ms para acumular edições rápidas contínuas (ex: arraste de sliders ou clipes).
   - Publicação assíncrona do snapshot imutável para `build/preview.bank` em segundo plano via `AsyncBankPublisher`.
   - Troca de banco *in-flight* sem reiniciar o Studio: captura o estado atual de reprodução (evento ativo, posição na timeline, valores de parâmetros e snapshots ligados), descarrega o banco anterior, carrega o novo banco e restaura a reprodução na mesma posição de forma transparente.
   - Proteção contra corridas e bancos órfãos: fechamento do projeto ou chegada de uma revisão mais nova cancela ou descarta o banco intermediário sem aplicá-lo ao engine.
3. **Deck de Mixer & Paridade com FMOD/Wwise (`MixerDeckPanel`)**:
   - Strips verticais para Master (`bus:/`), barramentos filhos (`bus:/...`) e VCAs (`vca:/...`).
   - Faders em decibéis (`-80.0 .. +10.0 dB`, com ganho linear 0.0 exato em `<= -80 dB`) e controles de pan azimutal (`-1.0 .. +1.0`).
   - Botões Mute (`M`) e Solo (`S`) por strip.
   - **Regra de Solo Hierárquico**: quando um barramento é solado, seus ancestrais e descendentes permanecem audíveis, enquanto barramentos irmãos e sub-árvores fora do caminho são silenciados.
   - Medidores balísticos verticais Peak e RMS em tempo real em cada strip, consultando `oae_bus_get_meter` a 30 Hz.
   - Barra de alternância de snapshots: botões de ativação de snapshots do projeto sincronizados via `setValueWithoutNotify` para evitar loops de evento.

---

## 2. Contratos de Audição & Máquina de Estados do Transporte

### 2.1 Estados de Reprodução
```cpp
enum class TransportState {
    Stopped,
    Playing,
    Paused
};
```

### 2.2 Transições de Estado
1. **Play (`play()`)**:
   - Se `Stopped`: obtém a descrição do evento atualmente selecionado no `AuthoringModel`, cria a instância `oae_event_instance`, aplica os valores atuais de parâmetros e executa `oae_event_instance_start`. O estado torna-se `Playing`.
   - Se `Paused`: executa `resume()`.
2. **Pause (`pause()`)**:
   - Se `Playing`: consulta a posição atual via `oae_event_instance_get_timeline_position_ms`, armazena `paused_position_ms_`, executa `oae_event_instance_stop(instance, OAE_STOP_IMMEDIATE)` e libera a instância. O estado torna-se `Paused`.
3. **Resume (`resume()`)**:
   - Se `Paused`: recria a instância do evento, aplica parâmetros, chama `oae_event_instance_set_timeline_position_ms(instance, paused_position_ms_)`, e chama `oae_event_instance_start`. O estado torna-se `Playing`.
4. **Stop (`stop(bool immediate = false)`)**:
   - Se `Playing` ou `Paused`: executa `oae_event_instance_stop` com `OAE_STOP_ALLOW_FADEOUT` (ou imediato se solicitado), reseta `paused_position_ms_ = 0` e o estado torna-se `Stopped`.
5. **Seek (`seek(double time_ms)`)**:
   - Se `Playing`: chama `oae_event_instance_set_timeline_position_ms(instance, static_cast<int32_t>(time_ms))`.
   - Se `Paused`: atualiza `paused_position_ms_ = time_ms`.
6. **KeyOff (`key_off()`)**:
   - Se `Playing`: despacha `oae_event_instance_key_off(instance)`.

---

## 3. Contrato do Debounced Preview Rebuild

### 3.1 Ciclo de Vida da Revisão
```text
[Edição no Modelo] ---> content_revision_++ ---> Inicia Timer 600 ms (Debounce)
                                                       |
                                               Timer dispara (após repouso)
                                                       |
                                               Gera Snapshot Imutável
                                                       |
                                          Compilação em Background (oae_bankc)
                                                       |
                                            Sucesso na compilação?
                                           /                      \
                                      SIM                         NÃO
                                       |                           |
                       Revisão ainda é a mais recente       Registra erro no console
                       e projeto ainda está carregado?      Mantém banco anterior
                                  /         \
                             SIM             NÃO (Projeto fechou ou nova rev em voo)
                              |               |
                    Captura Estado Atual      Descarta banco compilado
                              |
                    Troca Atômica no Engine
                    (Unload Antigo -> Load Novo)
                              |
                    Restaura Instância, Posição,
                    Parâmetros e Snapshots
```

### 3.2 Invariantes de Troca em Voo
- Nenhuma alocação ou operação bloqueante ocorre na thread de áudio.
- Se o usuário fechar o projeto enquanto a compilação estiver em andamento, o callback de término descarta o banco temporário e nunca carrega dados de um projeto já encerrado.

---

## 4. Contrato do Mixer & Regra de Solo Hierárquico

### 4.1 Regra de Solo Hierárquico
Para um conjunto de barramentos $B$ e um subconjunto de barramentos marcados com Solo $S \subseteq B$:
1. Se $S = \emptyset$, nenhum solo está ativo; o ganho de cada barramento obedece exclusivamente ao seu fader de volume e flag de mute individual.
2. Se $S \ne \emptyset$, para cada barramento $b \in B$:
   - $b$ permanece audível se e somente se:
     $$b \in S \quad \lor \quad (\exists s \in S \text{ tal que } b \text{ é ancestral de } s) \quad \lor \quad (\exists s \in S \text{ tal que } b \text{ é descendente de } s)$$
   - Caso contrário, $b$ é silenciado (ganho efetivo $= 0.0$).
3. O Master Bus (`bus:/`) é ancestral comum de todos os barramentos e, portanto, permanece sempre na rota ativa quando qualquer barramento filho é solado.

### 4.2 Sincronização de Snapshots
- Ao reconstruir os strips ou inicializar o Mixer, cada botão de alternância de snapshot consulta o estado real da instância do snapshot através de `oae_event_instance_get_playback_state` e atualiza a interface via `setToggleState(active, juce::dontSendNotification)`.

---

## 5. Critérios de Aceitação (AC)

1. **Transporte de Audição**: Play, Pause, Resume, Stop, Seek, KeyOff e ajustes de parâmetros locais e globais atuam com latência imperceptível e sem estalos de áudio.
2. **Rebuild Debounced**: Modificar propriedades de instrumentos ou pistas durante a reprodução dispara o rebuild de 600 ms, troca o banco e continua a reprodução na mesma posição da timeline sem reiniciar o Studio.
3. **Resistência a Concorrência**: Fechar um projeto com compilação em andamento não carrega bancos órfãos no engine.
4. **Mixer com Solo Hierárquico & VU Meters**: Os medidores verticais Peak/RMS respondem dinamicamente ao sinal de áudio, e a regra de solo mantém a cadeia ancestral/descendente audível enquanto silencia barramentos irmãos.
5. **Automação e Smoke Test**: O executável `oae_studio.exe --smoke-transport-mixer` executa de forma autônoma e aprova todas as etapas com exit code 0 em Debug e Release com zero avisos.
