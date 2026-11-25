# T04 — Hierarquia de buses

Cada sistema inicializado possui seu próprio Master Bus no caminho `bus:/`. Os buses agrupam canais e outros buses. O render aplica o ganho de cada bus no caminho até o Master.

| Operação | Contrato |
| --- | --- |
| `oae_system_get_master_bus` | Retorna o handle do Master. Consultar `bus:/` com `oae_system_get_bus` retorna o mesmo handle. |
| `oae_bus_create` | Cria um filho sob um pai do mesmo sistema. Passe `0` para usar o Master. O nome é um segmento UTF-8, sem separadores, `.` ou `..`. Um filho direto `sfx` tem caminho `bus:/sfx`. O limite é 32 níveis abaixo do Master. |
| `oae_system_get_bus` | Resolve um caminho completo e distingue maiúsculas de minúsculas. Retorna `OAE_ERR_NOT_FOUND` para um caminho válido ausente. |
| `oae_bus_set_volume` / `get_volume` | Controlam o ganho linear local, finito e não negativo. O padrão é 1. Um filho a 0,5 sob um pai a 0,5 aplica ganho composto de 0,25. |
| `oae_bus_set_mute` / `get_mute` | Controlam mute local com 0 ou 1. Mutar um pai silencia toda a subárvore. Os canais continuam avançando e o ganho configurado é preservado. |
| `oae_bus_set_paused` / `get_paused` | Controlam pausa local com 0 ou 1. Pausar um pai suspende a reprodução e o avanço dos seus descendentes. Despausar restaura a reprodução, respeitando as pausas locais dos filhos. |
| `oae_bus_stop_all` | Para os canais do bus e de todos os seus descendentes. Seus handles são reciclados no próximo `oae_system_update`. |
| `oae_bus_release` | Destrói um bus e sua subárvore. Os handles desses buses e de seus canais ficam inválidos imediatamente. Liberar o Master diretamente retorna `OAE_ERR_INVALID_STATE`. Use `oae_system_release` para encerrar o sistema e seu Master. |

`oae_system_play_sound` aceita um bus do mesmo sistema. O handle `0` é alias do Master apenas nas operações que o blueprint indica. Os getters de mute, volume e pausa retornam a configuração local de cada bus. Um canal pode estar iniciado e ter sua reprodução suspensa por um ancestral pausado.

Criar um segundo bus com o mesmo nome sob o mesmo pai retorna `OAE_ERR_ALREADY_EXISTS`. Nomes e caminhos malformados, ganho fora do domínio e profundidade acima de 32 retornam `OAE_ERR_INVALID_PARAM`. Handles inválidos ou de outro sistema retornam `OAE_ERR_INVALID_HANDLE`. Cada sistema precisa estar inicializado para criar ou buscar seus buses.

O grafo encadeia um grupo miniaudio e um strip por bus. Os grupos desabilitam o reamostrador de pitch. O pitch é aplicado por canal e o grupo não carrega amostras anteriores após mute ou pausa. O strip lê ganho e mute por atomics lock-free e processa o áudio sem alocar. A game thread modifica a topologia e libera canais antes dos buses, dos mais profundos até o pai.

## Exemplo em C

O exemplo encaminha um som já carregado no sistema a um novo bus `bus:/sfx`. Se a criação do canal falhar, ele libera o bus que acabou de criar.

```c
#include <oae_audio.h>

oae_result play_on_sfx(oae_system system, oae_sound sound, oae_bus* bus, oae_channel* channel)
{
    oae_result result = oae_bus_create(system, "sfx", 0, bus);
    if (result != OAE_OK) {
        return result;
    }
    result = oae_bus_set_volume(*bus, 0.5f);
    if (result == OAE_OK) {
        result = oae_system_play_sound(system, sound, *bus, 0, channel);
    }
    if (result != OAE_OK) {
        oae_bus_release(*bus);
    }
    return result;
}
```

Quando não precisar mais do grupo, chame `oae_bus_release(bus)`. Para parar somente a reprodução e manter a hierarquia, chame `oae_bus_stop_all(bus)`.
