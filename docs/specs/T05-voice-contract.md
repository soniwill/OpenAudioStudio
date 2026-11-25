# T05 — Gerenciamento e virtualização de vozes

`oae_init_settings.max_physical_voices` é o teto por sistema. Quando todos os slots físicos estão atribuídos, novos canais iniciam virtuais. Uma voz virtual fica desconectada do grafo e não gera áudio. A API mantém sua posição lógica com base nos frames renderizados, na taxa da fonte e no pitch. Pausa local ou de um ancestral congela essa posição. Loop volta ao início da fonte.

A cada `oae_system_update`, o sistema descarta canais encerrados e depois classifica os que continuam tocando:

1. Prioridade crescente, de 0 a 256, com 128 como padrão.
2. Audibilidade decrescente. É o ganho linear do canal multiplicado pelo ganho do bus e dos seus ancestrais. Qualquer mute ou pausa efetiva zera a audibilidade. A amplitude do arquivo não entra no cálculo. A partir da T06, os multiplicadores de fade correntes também compõem o ganho efetivo.
3. Ordem de criação crescente. Em empate, o canal mais antigo tem preferência.

A atualização rebaixa antes de promover para manter o teto. Ao promover uma voz, o sistema solicita seek para a posição virtual e anexa o canal ao grafo. Um stream pode emitir silêncio enquanto prepara a página solicitada. O relógio headless avança pelos frames pedidos à API, inclusive com saída silenciosa. No modo com dispositivo, avança a cada bloco entregue ao callback. Mudanças de estado no modo dispositivo são observadas em limites de bloco.

| Operação | Contrato |
| --- | --- |
| `oae_channel_set_priority` | Define prioridade de 0 a 256. Valores fora do intervalo retornam `OAE_ERR_INVALID_PARAM`. A classificação é aplicada no próximo update. |
| `oae_channel_get_priority` | Lê a prioridade configurada. |
| `oae_channel_is_virtual` | Retorna 1 se o canal não tem slot físico atribuído, ou 0 se tem. |
| `oae_system_get_channels_playing` | Retorna o total lógico de canais tocando e quantos deles têm slot físico. Um canal pausado diretamente fica fora da contagem. A pausa de bus suspende a execução sem alterar o estado lógico local de cada canal. |

Todos os getters exigem saídas válidas. Handles inválidos, reciclados ou de tipo diferente são rejeitados. Mantenha chamadas de controle na game thread, inclusive a consulta de posição.

## Exemplo em C

Depois de criar um canal, configure sua prioridade e atualize o sistema. O exemplo lê o estado após a classificação.

```c
#include <oae_audio.h>

oae_result prioritize(oae_system system, oae_channel channel, int* is_virtual)
{
    oae_result result = oae_channel_set_priority(channel, 16);
    if (result == OAE_OK) {
        result = oae_system_update(system);
    }
    if (result == OAE_OK) {
        result = oae_channel_is_virtual(channel, is_virtual);
    }
    return result;
}
```

Configure o teto antes de chamar `oae_system_init`. Use `oae_system_get_channels_playing` para observar total lógico e real a cada frame. Execute `./scripts/build.ps1 -Config Debug` ou `-Config Release` para validar o teto, a classificação, a pausa, o loop, o relógio e a promoção após descarte.
