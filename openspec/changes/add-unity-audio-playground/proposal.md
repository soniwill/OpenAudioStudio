# Proposta: Cena 3D Interativa Audio Playground na Unity (`add-unity-audio-playground`)

## Motivação
Validadores externos e sound designers avaliando o OpenAudioStudio como alternativa open-source ao FMOD/Wwise precisam de uma cena 3D universal em primeira pessoa (*Audio Playground / Showcase Room*) que demonstre e valide de ponta a ponta todas as capacidades do motor e da ponte Unity em um único ambiente interativo e replicável.

## Escopo
1. **Pipeline autônomo de aquisição, síntese e compilação de assets (`scripts/create_playground_assets.ps1`)**:
   - Aquisição de amostras reais sob licença CC0 (Domínio Público) via rede com conversão/padronização via `ffmpeg` (WAV PCM 16-bit, 48 kHz), geração de locuções faladas via `System.Speech.Synthesis.SpeechSynthesizer` e fallback determinístico offline em PowerShell para garantir reprodutibilidade em CI sem rede.
   - Geração de `example_project/playground_project.json` e `example_project/playground_vo_project.json` com áudio em `example_project/assets/playground/*.wav`, compilação de `playground.bank` e `playground_vo.bank` via `oae_bankc` e implantação em `unity/OpenAudioUnity/Assets/StreamingAssets/`.
2. **Controlador dual e sala 3D procedural (`AudioPlaygroundController.cs` em `unity/com.openaudio.core/Runtime/Components/`)**:
   - Compatível com compilação headless (`.NET 8`) e Unity Runtime, montando automaticamente a geometria 3D da sala com primitivas nativas da Unity, controlador em primeira pessoa (`WASD` + mouse look) com `StudioListener` e 6 estações de validação interativas:
     - **Estação 1 (Superfícies & Multi-Instrument)**: Pista com pisos de Madeira, Concreto e Grama acionando `event:/playground/footstep` com parâmetro discreto/labeled `SurfaceType` (`0`, `1`, `2`), randomização de pitch/volume e prevenção de efeito metralhadora.
     - **Estação 2 (Espacialização 3D, Cones & Built-in Parameters)**: Totem de Rádio/Fogueira com `StudioEventEmitter` (`event:/playground/emitter_3d`), roll-off configurável, cone direcional e leitura em tempo real dos Built-in Parameters (`Distance`, `Direction`, `Elevation`, `EventConeAngle`, `Speed`).
     - **Estação 3 (Música Adaptativa & Transições de Timeline)**: Zona de Arena que altera `CombatIntensity` (`0.0` Exploração <-> `1.0` Combate) em `event:/playground/adaptive_music`, validando marcadores, regiões de transição quantizadas e crossfade.
     - **Estação 4 (Ambientes Vivos & Scatterer Instrument)**: Bosque 3D com `event:/playground/forest_scatterer` emitindo grãos estocásticos espacializados com limite de polifonia e sound stealing.
     - **Estação 5 (Snapshots, Ducking, Banco sob Demanda & Programmer Instrument)**: Túnel acústico que ativa `snapshot:/playground/cave_reverb` (ou filtro low-pass) e totem de diálogo que carrega `playground_vo.bank` sob demanda e dispara `event:/playground/dialogue_vo` via `Programmer Instrument` (`audio_table`) com ducking sobre o barramento de música.
     - **Estação 6 (HUD 2D, VCAs & Live-Linking)**: Painel IMGUI/HUD com sliders para `vca:/playground_master`, `vca:/playground_music` e `vca:/playground_sfx`, botões de SFX 2D (`event:/playground/ui_click`) e telemetria/status do Live-Linking TCP (`127.0.0.1:7450`).
3. **Cena e suíte de testes automatizados**:
   - Cena `unity/OpenAudioUnity/Assets/Scenes/AudioPlayground.unity` configurada com `OaeAudioManager`, `StudioBankLoader` (`playground.bank`), `OaeDiagnosticsOverlay` e `AudioPlaygroundController`, registrada em `EditorBuildSettings.asset`.
   - Cobertura na suíte standalone `.NET` e no Unity PlayMode/EditMode validando todas as 6 estações, além de atualização documental no manual/README.

## Exclusões
- Nenhuma alteração na ABI C (`native/include/oae_audio.h`) ou no formato binário `.bank` v1.
- Sem dependência de pacotes externos da Unity Asset Store, URP/HDRP, ProBuilder ou modelos 3D `.fbx` proprietários.

## Capacidades Afetadas e Impacto por Camada
### Capacidades Afetadas (`New Capabilities` em `openspec/specs/`)
- `unity-bridge`: Inicia a migração canônica da capacidade de plataforma `unity-bridge`, formalizando os requisitos do pipeline autônomo de assets/bancos do Audio Playground e do controlador/cena 3D interativa para validação ponta a ponta (em substituição incremental ao legado de demo descrito em `docs/specs/README.md`).

### Impacto por Camada
- **Scripts / Pipeline de Assets (`scripts/`, `example_project/`)**: Novo script `scripts/create_playground_assets.ps1`, projetos JSON do Playground, assets `.wav` e bancos compilados.
- **Unity Bridge (`unity/com.openaudio.core/Runtime/Components/`, `unity/com.openaudio.core/Tests/`)**: Novo `AudioPlaygroundController.cs` e testes `.NET`/Unity PlayMode.
- **Cena & StreamingAssets (`unity/OpenAudioUnity/Assets/Scenes/`, `unity/OpenAudioUnity/Assets/StreamingAssets/`, `ProjectSettings/EditorBuildSettings.asset`)**: Nova cena `AudioPlayground.unity` e bancos implantados.
- **Documentação (`docs/`, `README.md`)**: Documentação de uso da cena Audio Playground e roteiro de validação.
