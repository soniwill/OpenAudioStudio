# Tasks — Audio Playground (`add-unity-audio-playground`)

## 1. Pipeline Autônomo de Assets e Bancos

- [x] 1.1 (`scripts/`, `example_project/`, `StreamingAssets`) Criar `scripts/create_playground_assets.ps1` com aquisição de amostras CC0 + normalização `ffmpeg` (WAV PCM 16-bit, 48 kHz) + locuções SAPI TTS (`System.Speech.Synthesis.SpeechSynthesizer`) + fallback determinístico offline em PowerShell, gerando `example_project/playground_project.json`, `example_project/playground_vo_project.json`, assets em `example_project/assets/playground/*.wav` e compilando `playground.bank` e `playground_vo.bank` (e catálogos JSON) para `unity/OpenAudioUnity/Assets/StreamingAssets/`. Aceite: executar `./scripts/create_playground_assets.ps1` com exit code 0 e confirmar presença dos bancos e catálogos gerados.

## 2. Controlador Dual e Cena 3D Audio Playground

- [x] 2.1 (`unity/com.openaudio.core/Runtime/Components/`) Implementar `AudioPlaygroundController.cs` (com `.meta`) suportando compilação headless (`.NET 8`) e geração procedural da sala 3D em primeira pessoa na Unity (controlador FPS + `StudioListener`, pistas de superfície Madeira/Concreto/Grama, totem 3D `StudioEventEmitter`, zona de Arena com `CombatIntensity`, bosque `Scatterer`, túnel `Snapshot` + totem de diálogo `Programmer Instrument` + `playground_vo.bank` sob demanda, HUD de VCAs e status Live-Linking TCP). Aceite: `./scripts/unity_test.ps1 -DotnetOnly` compila e carrega a nova classe sem erros.
- [x] 2.2 (`unity/OpenAudioUnity/Assets/Scenes/`, `ProjectSettings/`) Criar a cena `unity/OpenAudioUnity/Assets/Scenes/AudioPlayground.unity` (com `.meta`) configurada com `OaeAudioManager`, `StudioBankLoader` (`playground.bank`), `OaeDiagnosticsOverlay` e `AudioPlaygroundController`, e registrá-la em `unity/OpenAudioUnity/ProjectSettings/EditorBuildSettings.asset`. Aceite: abrir a cena no Unity Editor em modo batch ou PlayMode sem erros de console.

## 3. Testes Automatizados Ponta a Ponta

- [x] 3.1 (`unity/com.openaudio.core/Tests/`) Adicionar testes automatizados cobrindo as 6 estações do `AudioPlaygroundController` (`SurfaceType` e passos `Multi Instrument`, atualização 3D de listener/emissor e Built-in Parameters, `CombatIntensity`, `Scatterer`, `Snapshot` + carga de `playground_vo.bank` + `Programmer Instrument`, e níveis de VCA) na suíte standalone `.NET` e no Unity PlayMode. Aceite: `./scripts/unity_test.ps1 -TestPlatform Both` e `./scripts/deploy_unity.ps1` passam integralmente.

## 4. Documentação e Gates Finais

- [x] 4.1 (`docs/`, `openspec/`) Documentar a cena `AudioPlayground` e seu roteiro de validação rápida no manual/README (atualizando o apontamento de migração de `unity-bridge` em `docs/specs/README.md` quando apropriado) e executar `./scripts/verify.ps1 -All` validando `docs`, `native`, `studio` e `unity` com retorno `RESULT: OK`.
