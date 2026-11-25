# Contrato T15 — Unity Integration Package (`com.openaudio.core`)

## 1. Escopo e Propósito
Este documento define as especificações, a arquitetura e a conformidade do pacote Unity UPM **OpenAudio Engine Core** (`com.openaudio.core`), cobrindo as tarefas **T15a** e **T15b** do **Épico 4 (Integração Unity & Workflow de Jogo)**.

O objetivo do pacote é fornecer integração nativa, segura, com zero alocações periódicas no heap gerenciado, suporte estrito a AOT/IL2CPP e ergonomia equivalente a middlewares consolidados da indústria (FMOD Studio e Wwise).

---

## 2. Estrutura do Pacote UPM (`com.openaudio.core`)

O pacote está organizado no layout canônico do Unity Package Manager:

```text
unity/com.openaudio.core/
  package.json                          # Metadados do pacote UPM v1.0.0
  package.json.meta
  Runtime/
    OpenAudio.Runtime.asmdef             # Assembly Runtime com allowUnsafeCode: true
    OpenAudio.Runtime.asmdef.meta
    Core/
      OaeNative.cs                      # P/Invoke dos 100 métodos C e trampolim AOT/IL2CPP
      OaeTypes.cs                       # Enums e structs blittable FFI
      OaeException.cs                   # Exceções gerenciadas e asserções idiomáticas
      OaeSystem.cs                      # Wrapper orientado a objetos para o System nativo
      Bank.cs                           # Wrapper de banco Studio com IDisposable
      EventDescription.cs               # Metadados e fábrica de instâncias de eventos
      EventInstance.cs                  # Controle de reprodução, parâmetros e eventos de timeline
      Bus.cs                            # Controle de mixer, ganho, mute, pause, efeitos e metering
      Vca.cs                            # Controle de fader de VCAs
    Components/
      OaeAudioManager.cs                # Singleton MonoBehaviour DontDestroyOnLoad
      StudioBankLoader.cs               # Carregador de bancos síncrono/assíncrono
      StudioEventEmitter.cs             # Emissor de eventos com gatilhos de ciclo de vida e áudio 3D
      StudioListener.cs                 # Ouvinte de áudio 3D da cena
    Attributes/
      EventRefAttribute.cs              # Atributo [EventRef] para campos string
  Editor/
    OpenAudio.Editor.asmdef              # Assembly Editor isolado
    OpenAudio.Editor.asmdef.meta
    CatalogCache.cs                     # Cache de .catalog.json e validação CRC-32 IEEE 802.3
    EventRefDrawer.cs                   # PropertyDrawer com popup picker pesquisável
  Plugins/x86_64/
    oae_audio.dll                       # Binário nativo compilado do engine
    oae_audio.dll.meta                  # Configuração do PluginImporter para Windows x86_64
  Tests/
    Runtime/                            # Testes EditMode / PlayMode do Runtime
      OpenAudio.Runtime.Tests.asmdef
      NativeStructSizeTests.cs          # Validação FFI de tamanhos de structs com o engine
      StudioLifecycleTests.cs           # Testes de ciclo de vida, renderização e dsp_clock
      MarkerCallbackTests.cs            # Validação do trampolim estático de callbacks
    Editor/                             # Testes EditMode do Editor
      OpenAudio.Editor.Tests.asmdef
      CatalogCacheTests.cs              # Validação de parse, cache e integridade CRC-32
    StandaloneNet/                      # Projeto de teste autônomo via .NET CLI
      OpenAudio.StandaloneTests.csproj
```

---

## 3. Camada P/Invoke (`OaeNative.cs`)

### 3.1 Convenção de Chamada e Mapeamento Completo
- Todas as assinaturas nativas utilizam `[DllImport("oae_audio", CallingConvention = CallingConvention.Cdecl)]`.
- **100% dos 100 métodos da API C plana** exportados por `oae_audio.dll` estão declarados.
- Métodos com strings utilizam `CharSet.Ansi` correspondendo a `const char*` UTF-8 do C/C++.
- Buffers de saída float usam ponteiros fixados ou arrays marcados com `[Out]`.

### 3.2 Structs Blittable FFI e Verificação de Layout
As structs gerenciadas espelham byte-a-byte as estruturas nativas em `oae_audio.h`:

| Struct C# | Struct C Nativa | Tamanho x64 | Campos |
| :--- | :--- | :---: | :--- |
| `OaeInitSettings` | `oae_init_settings` | 16 bytes | `sample_rate`, `channels`, `max_physical_voices`, `flags` |
| `OaeMeter` | `oae_meter` | 72 bytes | `channel_count`, `frame_count`, `fixed float peak[8]`, `fixed float rms[8]` |
| `OaeTimelineMarkerProperties` | `oae_timeline_marker_properties` | 24 bytes | `dsp_clock`, `name` (IntPtr), `position_ms`, `marker_index` |
| `OaeTimelineBeatProperties` | `oae_timeline_beat_properties` | 32 bytes | `dsp_clock`, `bar`, `beat`, `position_ms`, `tempo`, `time_sig_upper`, `time_sig_lower` |
| `OaeEventSoundProperties` | `oae_event_sound_properties` | 32 bytes | `dsp_clock`, `sound`, `channel`, `instrument_index`, `nested_depth` |
| `OaeStructSizes` | `oae_struct_sizes` | 20 bytes | 5 campos `uint32_t` com tamanhos individuais |

O método `OaeNative.VerifyStructSizes()` invoca `oae_get_struct_sizes(&sizes)` na inicialização e valida em runtime que `Marshal.SizeOf<T>()` coincide com o tamanho esperado pela DLL. Se houver discrepância de packing ou alinhamento, uma `InvalidOperationException` detalhada é disparada impedindo corrupções de memória.

### 3.3 Trampolim Estático para AOT / IL2CPP
Para total conformidade com AOT no Unity IL2CPP (onde delegates nativos não podem ser delegates de instância):
- Um único método estático `NativeCallbackTrampoline` anotado com `[AOT.MonoPInvokeCallback(typeof(OaeEventCallback))]` recebe os disparos nativos:
  ```csharp
  [AOT.MonoPInvokeCallback(typeof(OaeEventCallback))]
  private static OaeResult NativeCallbackTrampoline(uint type, ulong instance, IntPtr properties, IntPtr userData)
  ```
- O trampolim consulta uma tabela de instâncias ativas (`ConcurrentDictionary<ulong, ManagedEventCallback>`) e despacha o callback para o `EventInstance` correspondente sem criar alocações nem boxing.
- Exceções gerenciadas são interceptadas dentro do trampolim, impedindo que falhas em scripts de gameplay se propaguem como exceções não capturadas para dentro do frame de render nativo.

---

## 4. Componentes de Cena Unity

### 4.1 `OaeAudioManager`
- Singleton `[DefaultExecutionOrder(-100)]` configurado com `DontDestroyOnLoad`.
- Inicializa o motor OAE nativo em `Awake()` respeitando taxa de amostragem, número de canais e teto de vozes físicas configuradas no Inspector.
- No `Update()`, executa periodicamente `oae_system_update(system)`.
- Gerencia perda e recuperação de foco via `OnApplicationFocus(bool)` e `OnApplicationPause(bool)`, pausando e retomando o `MasterBus` automaticamente para evitar reprodução contínua em background ou alt-tab se configurado `m_PauseOnFocusLoss`.
- No encerramento da cena ou jogo (`OnDestroy`), descarrega instâncias ativas e invoca `oae_system_release`.

### 4.2 `StudioBankLoader`
- Permite carregar um ou mais bancos Studio (.bank) localizados em `StreamingAssets` ou caminho absoluto.
- Triggers de carregamento configuráveis: `Awake`, `Start` ou `Manual`.
- Suporte a dois modos:
  - **Synchronous:** Executa `LoadBankFile` bloqueando até a leitura e decodificação completa.
  - **Asynchronous:** Utiliza `OAE_LOAD_BANK_NONBLOCKING` (`LoadBankFileAsync`) e uma corrotina Unity que monitora a transição de estado (`OAE_BANK_LOADING` -> `OAE_BANK_LOADED` ou `OAE_BANK_FAILED`), sem congelar o framerate da main thread.
- Descarregamento automático em `OnDestroy` invalidando handles e limpando referências nativas.

### 4.3 `StudioEventEmitter`
- Emissor integrado ao GameObject para reproduzir eventos autorados no Studio.
- Campo de evento decorado com `[EventRef]`.
- Disparo automático por gatilhos de física e ciclo de vida: `Awake`, `Start`, `OnEnable`, `OnTriggerEnter` ou `Manual`.
- Parada configurável em `OnDisable`, `OnDestroy`, `OnTriggerExit` ou `Manual` com modos `AllowFadeout` ou `Immediate`.
- Suporte a posicionamento 3D:
  - Quando `is3D == true`, monitora a posição em relação ao `StudioListener.CurrentListener`.
  - Calcula atenuação de volume linear entre `minDistance` e `maxDistance`.
  - Calcula o ângulo azimutal relativo ao eixo direito (`Right`) do ouvinte e aplica pan estéreo em tempo real.
- Suporte à parametrização inicial antes do início do som.

### 4.4 `StudioListener`
- Componente que define a posição e orientação dos ouvidos do jogador (geralmente na Main Camera).
- Registra `StudioListener.CurrentListener` automaticamente em `OnEnable()` e limpa em `OnDisable()`.

---

## 5. Ferramentas de Editor e Seletor `[EventRef]`

### 5.1 `CatalogCache`
- Rastreia arquivos `.bank.catalog.json` gerados pelo compilador de bancos (`oae_bankc`).
- Mantém cache em memória invalidado automaticamente por `LastWriteTimeUtc` e tamanho do arquivo.
- **Verificação de Integridade CRC-32 (IEEE 802.3):**
  - Lê o tamanho (`bank_size`) e o hash CRC-32 (`bank_crc32`) do catálogo.
  - Compara contra o arquivo binário `.bank` adjacente.
  - Detecta e reporta corrupção ou inconsistência entre o catálogo e o banco compilado.

### 5.2 `EventRefDrawer`
- Custom PropertyDrawer que substitui campos string por um seletor visual com botão `Browse...`.
- Ao clicar em `Browse...`, exibe um menu popup hierárquico pesquisável listando todos os eventos (`event:/...`) e snapshots (`snapshot:/...`) descobertos nos bancos locais de `StreamingAssets`.
- Permite selecionar facilmente o evento sem digitar strings manuais sujeitas a erros de digitação.

---

## 6. Scripts de Automação

### 6.1 `scripts/deploy_unity.ps1`
- Compila a engine nativa (`Release` ou `Debug`).
- Copia `oae_audio.dll` e `oae_audio.pdb` para `unity/com.openaudio.core/Plugins/x86_64/`.
- Gera ou atualiza o arquivo `oae_audio.dll.meta` configurando o `PluginImporter` com `isPreloaded: 1`, `CPU: x86_64` e ativação em Windows Standalone e Editor.

### 6.2 `scripts/unity_test.ps1`
- Executa a suíte de testes C# autônoma via `dotnet test` (execução em subsegundos).
- Se o Unity Editor estiver presente na máquina, dispara a execução dos testes EditMode e PlayMode em modo batchmode sem interface gráfica (`-batchmode -nographics -runTests`).

---

## 7. Evidências de Validação

- Compilação C# em .NET 8 e Unity 6 (6000.0.34f1) com **zero warnings e zero erros**.
- **Suíte Standalone .NET (`OpenAudio.StandaloneTests`):** 13/13 testes aprovados via `dotnet test`:
  - `NativeStructSizeTests.StructSizes_MatchNativeOaeStructSizesExactly`: `OaeInitSettings` (16B), `OaeMeter` (72B), `OaeTimelineMarkerProperties` (24B), `OaeTimelineBeatProperties` (32B), `OaeEventSoundProperties` (32B).
  - `NativeStructSizeTests.VerifyStructSizes_DoesNotThrow`.
  - `NativeStructSizeTests.NativeVersion_ReturnsExpectedFormat`.
  - `StudioLifecycleTests.MasterBus_CanGetAndSetProperties`.
  - `StudioLifecycleTests.HeadlessRendering_WithSilentBus_RendersZeroes`.
  - `StudioLifecycleTests.DspClock_AdvancesOverUpdates`.
  - `StudioLifecycleTests.GlobalParameter_CanSetAndGet`.
  - `MarkerCallbackTests.TrampolineRegistration_DoesNotThrow`.
  - `CatalogCacheTests.Crc32_MatchesStandardIEEE8023`.
  - `CatalogCacheTests.ParseCatalogJson_ExtractsFieldsAndEvents`.
  - `CatalogCacheTests.LoadCatalog_WithValidBank_ValidatesCrcCorrectly`.
  - `CatalogCacheTests.LoadCatalog_WithCorruptedBank_ReportsInvalidCrc`.
  - `StudioPlayModeTests.Standalone_PlayMode_Placeholder`.
- **Suíte Unity Batchmode EditMode (`results_EditMode.xml`):** 12/12 testes aprovados no Unity Editor 6000.0.34f1:
  - `CatalogCacheTests` (4/4 aprovados).
  - `MarkerCallbackTests` (1/1 aprovado).
  - `NativeStructSizeTests` (3/3 aprovados).
  - `StudioLifecycleTests` (4/4 aprovados).
- **Suíte Unity Batchmode PlayMode (`results_PlayMode.xml`):** 10/10 testes aprovados na engine de simulação de cena do Unity:
  - `OaeAudioManager_InitializesAndUpdatesInScene`: ciclo de vida completo do Singleton `OaeAudioManager`, inicialização nativa e atualização contínua no loop de frames.
  - `StudioEventEmitter_CanAttachAndPlay`: acoplamento em GameObject, parametrização, disparo e parada controlada.
  - Testes de runtime do motor nativo rodando dentro do domínio PlayMode do Unity.
