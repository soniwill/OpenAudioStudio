# T03 — Sons e canais

A API carrega WAV, MP3, FLAC e OGG Vorbis. Um som é a fonte de áudio. Cada reprodução cria um canal com posição, pausa, volume e pitch próprios. Chame a API na game thread ou message thread do sistema.

| Operação | Contrato |
| --- | --- |
| `oae_sound_create_from_file` | Recebe um caminho UTF-8. No modo padrão, decodifica o arquivo antes de retornar. Com `OAE_SOUND_STREAM`, lê e decodifica páginas de áudio em segundo plano durante a reprodução. Mantenha o arquivo disponível e inalterado até o fim dos canais que o usam. |
| `oae_sound_create_from_memory` | Decodifica os bytes antes de retornar. O chamador pode liberar a memória após a chamada. Rejeita `OAE_SOUND_STREAM` com `OAE_ERR_INVALID_PARAM`. |
| `oae_sound_get_length_ms` | Informa a duração original, em milissegundos inteiros, independentemente de pitch ou loop. |
| `oae_system_play_sound` | Cria um canal a partir de um som do próprio sistema. Use bus `0` para o master e `paused` igual a 0 ou 1. |
| `oae_sound_release` | Invalida o handle do som. Canais ativos seguem tocando e mantêm os dados até o descarte do último canal. |
| `oae_channel_set_paused` / `get_paused` | Pausa ou retoma o canal sem reiniciar a posição. O valor é 0 ou 1. |
| `oae_channel_set_volume` / `get_volume` | Ganho linear finito e não negativo. O valor inicial é 1. Um ganho de 0,5 reduz a amplitude à metade. |
| `oae_channel_set_pitch` / `get_pitch` | Multiplicador finito e positivo, com 1 como valor inicial e 2 para o dobro da velocidade. A razão entre fonte e saída também precisa caber na representação numérica do reamostrador. |
| `oae_channel_set_position_ms` / `get_position_ms` | Consulta ou solicita posição na fonte, em milissegundos. Posições após a duração são limitadas ao fim. A consulta pode refletir um seek pendente, aplicado na próxima renderização do canal. |
| `oae_channel_is_playing` | Retorna 1 para um canal que está tocando e 0 se pausado, parado ou encerrado. |
| `oae_channel_stop` | Para o canal. O próximo `oae_system_update` recicla o handle de canais parados ou encerrados. |

Combine `OAE_SOUND_LOOP` com o modo padrão ou com streaming de arquivo para repetir a fonte. Enquanto o canal estiver ativo, ele pode continuar após a liberação do som. Ao liberar o sistema, todos os sons e canais que pertencem a ele são invalidados.

Parâmetros inválidos retornam `OAE_ERR_INVALID_PARAM`. Handles de tipo incorreto, liberados ou de outro sistema retornam `OAE_ERR_INVALID_HANDLE`. Arquivo ausente retorna `OAE_ERR_FILE_NOT_FOUND`. Arquivo ou bytes que o decoder não reconhece retornam `OAE_ERR_FORMAT`. Tentativas de pausar ou buscar em um canal já encerrado retornam `OAE_ERR_INVALID_STATE` até seu handle ser reciclado.

A decodificação preserva o sample rate e o layout de canais da fonte. O engine converte para o formato de saída. Cada canal usa seu próprio cursor. Em streaming, um bloco pode sair em silêncio enquanto o worker prepara a próxima página após um seek. Quando o decoder não informa a duração, a criação conta os frames em blocos na game thread e descarta o PCM de cada bloco.

## Exemplo em C

Este trecho inicia um WAV em um sistema já inicializado e libera o handle do som. O canal continua válido e a aplicação segue chamando `oae_system_update` uma vez por frame.

```c
#include <oae_audio.h>

oae_result play_wav(oae_system system, const char* path, oae_channel* channel)
{
    oae_sound sound = 0;
    oae_result result = oae_sound_create_from_file(system, path, OAE_SOUND_DEFAULT, &sound);
    if (result != OAE_OK) {
        return result;
    }
    result = oae_system_play_sound(system, sound, 0, 0, channel);
    const oae_result released = oae_sound_release(sound);
    return result == OAE_OK ? released : result;
}
```

Para um arquivo longo, passe `OAE_SOUND_STREAM` na criação. Para bytes já lidos pela aplicação, passe o endereço e o tamanho a `oae_sound_create_from_memory`, com `OAE_SOUND_DEFAULT` ou `OAE_SOUND_LOOP`.

## Testes e arquivos de áudio

Os testes usam tons sintéticos de 440 Hz e 44,1 kHz. O WAV de 250 ms foi gerado por síntese senoidal e convertido com FFmpeg para FLAC, MP3 e OGG Vorbis. O arquivo `stream.ogg` repete o tom por três segundos para exercitar seek entre páginas. O build usa esses arquivos versionáveis e dispensa FFmpeg na execução.

```powershell
./scripts/build.ps1 -Config Debug
./scripts/build.ps1 -Config Release
```
