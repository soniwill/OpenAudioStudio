# Contrato T14 — C API Hardening, Async Bank Loading & Sample Management

## 1. Escopo e Propósito
Este documento consolida a especificação, as regras de segurança e a validação de conformidade da ABI C de runtime do OpenAudioEngine para as tarefas T14a, T14b e T14c, concluindo o **Épico 3 (Bank Serialization & CLI Compiler)**.

## 2. Assinaturas e Estruturas C ABI
Declaradas em `native/include/oae_audio.h` e exportadas com convenção `cdecl` (`OAE_CALL`):

### 2.1 Verificação de Layout FFI
```c
typedef struct oae_struct_sizes {
    uint32_t init_settings;
    uint32_t meter;
    uint32_t timeline_marker_properties;
    uint32_t timeline_beat_properties;
    uint32_t event_sound_properties;
} oae_struct_sizes;

OAE_API oae_result OAE_CALL oae_get_struct_sizes(oae_struct_sizes* out_sizes);
```
- Retorna `OAE_ERR_INVALID_PARAM` se `out_sizes == NULL`.
- Preenche exatamente os tamanhos `sizeof(...)` para verificação de compatibilidade blittable por bindings como C# P/Invoke e Rust.

### 2.2 Carregamento Assíncrono de Bancos
```c
enum { OAE_LOAD_BANK_NONBLOCKING = 1u << 0 };

typedef enum oae_bank_loading_state {
    OAE_BANK_LOADING = 0,
    OAE_BANK_LOADED  = 1,
    OAE_BANK_FAILED  = 2
} oae_bank_loading_state;

OAE_API oae_result OAE_CALL oae_studio_load_bank_file_ex(oae_system system, const char* path, uint32_t flags, oae_bank* out_bank);
OAE_API oae_result OAE_CALL oae_studio_load_bank_memory_ex(oae_system system, const void* bytes, size_t size, uint32_t flags, oae_bank* out_bank);
OAE_API oae_result OAE_CALL oae_studio_get_bank_loading_state(oae_system system, oae_bank bank, int* out_state);
```
- Se `flags & OAE_LOAD_BANK_NONBLOCKING`, o banco é alocado imediatamente no runtime com estado `OAE_BANK_LOADING` e o handle é devolvido sem bloquear o chamador.
- Uma thread em background executa o parsing e registro assíncrono.
- `oae_system_update` nunca é bloqueado por mais de 1 ms durante o carregamento assíncrono.
- Falhas de sintaxe/formato ou IO transicionam atomicamente o estado para `OAE_BANK_FAILED`.

### 2.3 Gerenciamento Explícito de Sample Data
```c
OAE_API oae_result OAE_CALL oae_event_description_load_sample_data(oae_event_description description);
OAE_API oae_result OAE_CALL oae_event_description_unload_sample_data(oae_event_description description);
```
- Bancos carregados com `OAE_LOAD_BANK_NONBLOCKING` iniciam com contagem de pinos de sample em 0.
- `oae_event_instance_start` chamado em evento com samples descarregados retorna `OAE_ERR_INVALID_STATE`.
- `oae_event_description_load_sample_data` incrementa a contagem de pinos de cada sample único usado pelo evento, instanciando o `oae_sound` apenas quando a contagem atinge 1.
- `oae_event_description_unload_sample_data` decrementa a contagem de pinos, liberando o buffer PCM nativo (`oae_sound_release`) quando a contagem atinge 0.

### 2.4 Proteção de Reentrância (`in_callback`)
- Durante a execução de callbacks de eventos (ex: marcadores, batidas), a flag `in_callback` do `System` protege o estado interno.
- As funções `oae_system_update`, `oae_system_render`, `oae_system_release`, `oae_studio_unload_bank`, `oae_event_description_load_sample_data` e `oae_event_description_unload_sample_data` retornam `OAE_ERR_INVALID_STATE` se invocadas a partir de dentro do callback.

### 2.5 Total de Exportações da DLL
- A biblioteca dinâmica `oae_audio.dll` exporta exatamente **100 funções C**, todas com prefixo `oae_`.

## 3. Validação Executada
- **Suíte de Testes:** `native/tests/api_hardening.cpp`
  - `T14a - Struct Sizes Layout Verification`: conformidade com `sizeof`.
  - `T14b - Async Bank Loading & Nonblocking State Machine`: transição `LOADING -> LOADED` sem travar `update` (> 1ms) e transição `LOADING -> FAILED` em dados corrompidos.
  - `T14b - Explicit Sample Data Management`: `start` antes do preload retorna `OAE_ERR_INVALID_STATE`; após preload renderiza áudio real; pin count compartilhado entre 2 eventos funcionando sem vazamento.
  - `T14b - In-Callback State Guard`: 6 chamadas bloqueadas com `OAE_ERR_INVALID_STATE` durante despacho de callbacks.
  - `T14a - DLL Export Symbols Audit`: auditoria da tabela de exportação PE da DLL validando 100 símbolos iniciando estritamente com `oae_`.
  - `T14a - C API Fuzzing Robustness`: 10.000 iterações de fuzzing aleatório com ponteiros nulos, handles lixo e floats NaN/Inf sem crash.
- **Gates:**
  - Debug (`/W4 /WX`): 100% aprovado.
  - Release (`/W4 /WX`): 100% aprovado.
