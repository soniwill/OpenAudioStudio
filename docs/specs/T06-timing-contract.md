# T06 — Pan, fades e agendamento

O relógio DSP é expresso em frames da saída. Avança durante o silêncio. Em modo headless, começa em zero e avança pelo número de frames pedidos em cada render bem-sucedido.

| Operação | Contrato |
| --- | --- |
| `oae_system_get_dsp_clock` | Lê o relógio em frames. O sistema precisa estar inicializado. |
| `oae_system_play_sound_at` | Agenda a reprodução em um frame absoluto. Se o frame já passou, inicia no próximo frame renderizável. |
| `oae_system_play_sound_delayed` | Agenda a reprodução após um intervalo em milissegundos, arredondado ao frame mais próximo. |
| `oae_channel_set_pan` / `get_pan` | Controlam o pan do canal de −1 a +1. Centro tem ganho unitário. Hard pan silencia exatamente o lado oposto. O getter devolve o alvo. |
| `oae_bus_set_pan` / `get_pan` | Aplicam o mesmo pan após a soma do bus. |
| `oae_channel_fade_to` / `oae_bus_fade_to` | Iniciam um novo multiplicador de ganho a partir do valor de fade corrente. Aceitam LINEAR, LOG e SCURVE. Duração zero aplica o alvo imediatamente. |
| `oae_channel_get_fade_gain` / `oae_bus_get_fade_gain` | Avaliam o multiplicador do fade no relógio DSP atual. O fader de volume e o mute são controles independentes. |
| `oae_channel_stop_with_fade` | Aplica fade LINEAR para zero e para no frame final. Duração zero para imediatamente. O próximo update recicla o handle. |

O agendamento aceita até uma hora no futuro. Como em `oae_system_play_sound`, o bus `0` é o Master, e o som e o bus precisam pertencer ao mesmo sistema. Um canal agendado pode reservar uma voz física antes do início. Se estiver virtual, seu cursor não avança antes do frame solicitado. Quando um canal é promovido, ele respeita o agendamento restante ou retoma da posição virtual. Streams podem ter silêncio enquanto o worker prepara a página de origem. Canais com taxa igual à saída começam sem latência do reamostrador em pitch unitário. Alterar o pitch ativa o reamostrador pré-alocado, cujo processamento pode introduzir sua latência de filtro.

A primeira configuração recebida pelo strip é aplicada imediatamente. Alterações seguintes de pan e volume de bus passam por uma rampa linear de 128 frames a 48 kHz, escalada pela taxa de amostragem com mínimo de 16 frames. O volume de canal não usa essa rampa. Em mono, pan não muda a saída. Em saídas com mais de dois canais, pan atua apenas nos dois primeiros.

Os fades avançam pelo relógio DSP mesmo sob pausa. A curva LOG usa piso de −80 dB entre os extremos e retorna o alvo exato ao terminar. Mute continua produzindo silêncio exato. O callback processa blocos de até 256 frames e conserva a última configuração válida de fade se a leitura coincidir com uma atualização. Nenhuma espera é feita nesse caso.

Pan, alvo de fade e curva fora do domínio retornam `OAE_ERR_INVALID_PARAM`. O ganho alvo é linear, finito e não negativo. Um atraso acima de uma hora também é inválido. Handles liberados, incorretos ou de outro sistema retornam `OAE_ERR_INVALID_HANDLE`.

## Exemplo em C

Agende um som já carregado para daqui a 120 ms e envie-o à esquerda. Se a configuração falhar depois de criar o canal, pare-o para permitir que o próximo update o descarte.

```c
#include <oae_audio.h>

oae_result schedule_left(oae_system system, oae_sound sound, oae_channel* channel)
{
    oae_result result = oae_system_play_sound_delayed(system, sound, 0, 120, channel);
    if (result != OAE_OK) {
        return result;
    }
    result = oae_channel_set_pan(*channel, -1.0f);
    if (result != OAE_OK) {
        oae_channel_stop(*channel);
    }
    return result;
}
```

Para iniciar dois canais em frames fixos, leia o relógio uma vez, calcule os dois frames e passe ambos a `oae_system_play_sound_at`. Para alterar o ganho gradualmente, use `oae_channel_fade_to(channel, 0.5f, 500, OAE_FADE_SCURVE)`. Continue chamando `oae_system_update` a cada frame da aplicação.
