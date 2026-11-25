# Sistema de áudio e renderização headless

A versão inicial da API C reporta `1` em `oae_get_version()`. Use `oae_result_string(result)` para converter um código de resultado em texto estático. A string pertence à biblioteca.

1. Chame `oae_init_settings_default` para preencher uma struct do chamador. Os valores iniciais são 48.000 Hz, dois canais, 32 vozes físicas e modo com dispositivo. Taxa e número de vozes devem ser positivos. O limite de saída é oito canais, correspondente ao limite previsto para medição.
2. Chame `oae_system_create` para obter um handle. Há até 64 sistemas simultâneos nesta implementação. A falta de slots retorna `OAE_ERR_OUT_OF_MEMORY`.
3. Chame `oae_system_init` com as configurações. Use `OAE_INIT_NO_DEVICE` para renderização offline. O modo padrão abre o dispositivo pelo miniaudio. Um segundo init bem-sucedido é recusado com `OAE_ERR_ALREADY_INITIALIZED`.
4. Chame `oae_system_update` uma vez por frame. A função verifica o estado de inicialização, recicla canais parados ou encerrados e classifica vozes físicas e virtuais. Consulte os contratos de [sons e canais](T03-sound-contract.md) e de [vozes](T05-voice-contract.md).
5. Chame `oae_system_render` apenas no modo sem dispositivo. Reserve `frames * channels` floats de saída intercalados. Sem fontes, o resultado é silêncio exato. A função retorna `OAE_ERR_DEVICE` para um sistema inicializado em modo dispositivo. Um buffer nulo é inválido mesmo com zero frames.
6. Chame `oae_system_release` para encerrar um sistema criado, inclusive se a inicialização tiver falhado. O handle deixa de ser válido.

Chame a API pública na game thread ou message thread. O callback de dispositivo interno do miniaudio roda separadamente. Não chame `render` e `release` concorrentemente: o lookup é lock-free, mas o contrato de thread única protege a vida útil do ponteiro. Argumentos de domínio inválidos são verificados antes de handles. Antes de init, update e render retornam `OAE_ERR_NOT_INITIALIZED`.

Exemplo em C para link com `oae_audio`:

```c
#include <oae_audio.h>

int render_silence(void)
{
    oae_init_settings settings;
    oae_system system = 0;
    if (oae_init_settings_default(&settings) != OAE_OK ||
        oae_system_create(&system) != OAE_OK) {
        return 1;
    }

    settings.flags = OAE_INIT_NO_DEVICE;
    float samples[256 * 2];
    oae_result result = oae_system_init(system, &settings);
    if (result == OAE_OK) {
        result = oae_system_update(system);
    }
    if (result == OAE_OK) {
        result = oae_system_render(system, samples, 256);
    }
    const oae_result released = oae_system_release(system);
    return result == OAE_OK && released == OAE_OK ? 0 : 1;
}
```

Para link estático manual, defina `OAE_AUDIO_STATIC` no consumidor. O target CMake `oae_audio_static` propaga essa definição. O teste `c_api_smoke` compila como C11 e verifica o link dinâmico.

## Handles e limite de geração

Cada handle de sistema carrega índice de 32 bits, geração de 24 bits e tag de tipo de oito bits. O lookup rejeita tags e gerações incorretas, índices fora da tabela e slots vazios. Ao liberar o slot, a geração aumenta e volta a 1 depois de `0xFFFFFF`.

Ponto de investigação aprovado pelo usuário. Após 16.777.215 liberações do mesmo slot, um handle antigo pode coincidir com um novo handle do mesmo tipo. Avaliar a taxa real de reutilização e o impacto de manter handles antigos por longos períodos. Qualquer mudança da política de retorno a 1 requer revisar o contrato do blueprint.

O teste de renderização usa um guard por thread para contar chamadas C++ de alocação e liberação e exige zero operações. Esse guard não intercepta chamadas C de `malloc` ou alocações internas do sistema operacional.
