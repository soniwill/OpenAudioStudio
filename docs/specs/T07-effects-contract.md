# T07 — Efeitos e medição

Cada bus tem uma cadeia pré-fader e outra pós-fader, com até oito efeitos em cada. A ordem é efeitos pré-fader, volume/pan/fade, efeitos pós-fader, mute, medidor e sidechain. O mute silencia inclusive caudas de delay e reverb. Um bus pausado suspende o processamento da sua cadeia.

| API | Comportamento |
| --- | --- |
| `oae_bus_add_effect` | Insere na cadeia pós-fader. |
| `oae_bus_add_effect_ex` | Escolhe `OAE_EFFECT_PRE_FADER` ou `OAE_EFFECT_POST_FADER`. O índice começa em zero. `OAE_EFFECT_INDEX_LAST` acrescenta ao fim. Inserir no meio desloca os efeitos seguintes. Uma cadeia cheia retorna `OAE_ERR_INVALID_STATE`. |
| `oae_effect_set_param` / `get_param` | Escrevem ou leem um parâmetro definido para o tipo. Valores não finitos, IDs inválidos e valores fora da faixa retornam `OAE_ERR_INVALID_PARAM`. |
| `oae_effect_set_bypass` / `get_bypass` | Controlam bypass com 0 ou 1. Em bypass, a cadeia recebe o sinal intacto e o estado do efeito fica suspenso. |
| `oae_effect_set_wet_dry` / `get_wet_dry` | Ajustam independentemente os ganhos de sinal processado e original entre 0 e 1. O padrão é wet 1 e dry 0. Wet 0 e dry 1 preservam o sinal bit a bit e suspendem o processamento. |
| `oae_effect_set_sidechain` | Configura o compressor para ler outro bus do mesmo sistema. O compressor começa com detector interno. Use um handle válido e diferente do bus do compressor para ligar um sidechain. |
| `oae_effect_release` | Remove o efeito da cadeia e invalida seu handle. Liberar o bus também invalida todos os seus efeitos. |
| `oae_bus_get_meter` | Retorna a última janela fechada por `oae_system_update`. Consulte `channel_count`, `frame_count`, `peak[c]` e `rms[c]`. |

Handles inválidos ou pertencentes a outro sistema retornam `OAE_ERR_INVALID_HANDLE`. Callbacks de áudio não usam essa API. A game thread aplica mudanças de topologia e fecha as janelas de medição.

## Efeitos e parâmetros

Os enums de parâmetros e suas unidades estão em `oae_audio.h`. Os números abaixo indicam faixa e valor inicial.

| Efeito | Parâmetros |
| --- | --- |
| LIMITER | Teto −12 a 0 dBFS, inicial −0,1. Release 1 a 1000 ms, inicial 50. Ganho de entrada 0 a 24 dB, inicial 0. O look-ahead é de 1,5 ms. |
| COMPRESSOR | Threshold −60 a 0 dB, inicial −20. Ratio 1 a 20, inicial 4. Attack 0,1 a 500 ms, inicial 10. Release 1 a 5000 ms, inicial 100. Makeup 0 a 24 dB, inicial 0. |
| EQ3 | Ganhos de low, mid e high de −80 a 12 dB, inicialmente 0. Crossovers de 20 a 20000 Hz, inicialmente 400 e 4000 Hz. O crossover inferior deve ser menor que o superior. |
| LOWPASS e HIGHPASS | Corte de 10 a 20000 Hz, inicialmente 20000 e 10 Hz, respectivamente. Q de 0,1 a 10, inicial 0,7071. O processamento limita o corte abaixo de Nyquist. |
| REVERB | Room size, damping e width entre 0 e 1, inicialmente 0,5, 0,5 e 1. A rede Freeverb usa oito combs e quatro all-passes por canal. |
| DELAY | Tempo entre 1 e 2000 ms, inicial 300. Feedback de 0 a 0,95, inicial 0. |

Os biquads seguem as equações do [Audio EQ Cookbook](https://www.w3.org/TR/audio-eq-cookbook/). O EQ3 usa pares Butterworth em cascata e alinha a fase entre as três bandas, conforme a topologia de [Linkwitz-Riley de quarta ordem](https://www.linkwitzlab.com/filters.htm). Os atrasos de reverb partem da [afinação Freeverb de Jezar](https://github.com/boochow/freeverb-logue/blob/main/tuning.h). Os filtros atualizam coeficientes em sub-blocos de 32 frames, com frequência interpolada em log, ganhos em dB e Q linear. A primeira configuração ou saída de bypass aplica os parâmetros atuais sem rampa.

O sidechain usa o pico do bloco anterior do bus fonte, depois do fader, dos efeitos e do mute. O atraso impede que o resultado dependa da ordem de travessia. Mutar a fonte encerra a entrada de controle após um bloco e o compressor volta ao ganho unitário conforme seu release, antes do makeup. Se a fonte for removida, o sidechain permanece silencioso até o sidechain ser reatribuído ou o efeito ser liberado. O medidor e o sidechain contam NaN e infinito como zero e limitam a magnitude medida a 1e9.

## Exemplo em C

O exemplo coloca um compressor pós-fader em `music` e usa `voice` como sinal de controle.

```c
#include <oae_audio.h>

oae_result duck_music(oae_bus music, oae_bus voice, oae_effect* compressor)
{
    oae_result result = oae_bus_add_effect(music, OAE_EFFECT_COMPRESSOR,
                                         OAE_EFFECT_INDEX_LAST, compressor);
    if (result != OAE_OK) {
        return result;
    }
    result = oae_effect_set_param(*compressor, OAE_COMPRESSOR_THRESHOLD_DB, -18.0f);
    if (result == OAE_OK) {
        result = oae_effect_set_sidechain(*compressor, voice);
    }
    if (result != OAE_OK) {
        oae_effect_release(*compressor);
    }
    return result;
}
```

Use `oae_effect_get_param` para ler o threshold e `oae_effect_set_bypass(compressor, 1)` para comparar com o sinal original. Para um eco, crie `OAE_EFFECT_DELAY`, ajuste `OAE_DELAY_TIME_MS` e use `oae_effect_set_wet_dry(delay, 0.25f, 1.0f)`. Depois de renderizar e chamar `oae_system_update`, use `oae_bus_get_meter(music, &meter)` para consultar o pico e o RMS. Libere os efeitos ao concluir.

Os [testes headless](../../native/tests/bus_effects.cpp) incluem exemplos reproduzíveis dos sete tipos, medição, ordem de cadeia, bypass e sidechain.
