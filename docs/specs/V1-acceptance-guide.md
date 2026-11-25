# Guia de Aceite e Operação — OpenAudioStudio V1 para Jogos 2D

## 1. Fluxo Studio → Banco → Unity

1. **Autoria no Studio (`OpenAudio Studio.exe`):**
   - Marque músicas e ambiências longas com **Stream from Disk** (`"stream": true` em `samples[]` do `project.json`) para evitar carregar PCM completo na RAM.
   - Mantenha SFX curtos sem streaming para disparo imediato em memória.
   - Configure o **Master Bank** (`playground_project.json` -> `playground.bank`) com os barramentos de categoria (`bus:/music`, `bus:/sfx`, `bus:/ui`, `bus:/ambience`), snapshots (`snapshot:/playground/cave_reverb`, `snapshot:/playground/vo_ducking`), parâmetros globais e VCAs.
   - Configure **Content Banks** adicionais (`playground_vo_project.json` -> `playground_vo.bank`) referenciando os mesmos `bus_path` do Master Bank (conforme [Decisão 002](../decisions/002-padrao-master-bank-mixer-fmod-wwise.md)).
2. **Publicação e Diagnóstico de Erros:**
   - Clique em **Build Bank** (`Ctrl+B`) ou execute `oae_bankc.exe <projeto.json> -o <saida.bank>`.
   - Campos inválidos retornam diagnóstico com JSON Pointer (ex.: `Error at /events/0/bus_path: Unknown bus...`), preservando intactos o `.bank` e o `.bank.catalog.json` válidos anteriores.
3. **Ordem de Carregamento na Unity:**
   - Carregue o **Master Bank** (`playground.bank`) primeiro via `StudioBankLoader` ou `OaeSystem.LoadBankFile`.
   - Carregue **Content Banks** (`playground_vo.bank`) sob demanda (ex.: entrada em nova fase ou diálogo) e descarregue com `Bank.Unload()` após as caudas.

## 2. Componentes Unity 2D & Diagnóstico

- **`StudioEventEmitter`:**
  - Suporta `Collider` (3D) e `Collider2D` (2D) via `OnTriggerEnter2D` / `OnTriggerExit2D` com `StopMode = AllowFadeout`.
  - `SpatialMode = Spatial2D` com flags opcionais `Enable2DPan`, `Enable2DAttenuation` e referência selecionável `ListenerOverride`.
- **`OaeDiagnosticsOverlay` (Development Build / Editor):**
  - Pressione `F9` para exibir/ocultar o HUD com vozes reais/virtuais, bancos carregados, eventos ativos e níveis por barramento (`Peak dBFS`, `RMS dBFS` e `K-14 VU`).
- **`AudioPlaygroundController`:**
  - Transições musicais sincronizadas ao relógio DSP nativo (`OaeSystem.GetDspClock()`).
  - Controles para troca de zonas adaptativas, carregamento sob demanda do banco de voz, pausa e cenários reproduzíveis.

## 3. Comandos de Validação

- Motor nativo & streaming/loop: `./scripts/build.ps1 -Config Release`
- Studio & VU/balística (`--smoke-vu`): `./scripts/build_studio.ps1 -Config Release`
- Suíte C# Standalone: `./scripts/unity_test.ps1 -DotnetOnly`
- Suíte Unity Editor (EditMode + PlayMode): `./scripts/unity_test.ps1`
