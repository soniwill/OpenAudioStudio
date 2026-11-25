# Evidências de Verificação — `playground-video-tutorials`

Data: 2026-11-19

## 1. Evidências Visuais ao Vivo (`OpenAudio Studio.exe --mcp example_project/playground_project.json`)

Capturas geradas via `node scripts/tutorial_studio_cue.js --episode <EP> --step <STEP> --screenshot-dir openspec/changes/playground-video-tutorials/evidence` (utilizando `studio_ui_highlight` + `studio_ui_screenshot`) e limpas ao final com `--clear`:

| Evidência PNG | Modo / Alvo | Validação Visual |
| :--- | :--- | :--- |
| 03A-1.png (evidência local) | **Timeline** (`timeline_view` em `event:/playground/adaptive_music`) | Scrim escuro em 4 retângulos, contorno ciano e pílula `"Timeline: Exploration (0-16s), Combat (18-34s) e Tension"` legível sem corte. |
| 03B-3.png (evidência local) | **Layers** (`layers.card.1.mask_canvas` após `timeline.mode.layers`) | Spotlight âmbar exatamente sobre a `MaskRangeCanvas` da camada 2 (`Tension`), exibindo `"Curva Personalizada"` / `"Editar na Timeline"` e pílula inteira. |
| 00A-2.png (evidência local) | **Mixer Deck** (`deck_mixer` após `btn_tab_mixer`) | Spotlight ciano cobrindo todo o Mixer Deck (Master Limiter, 5 buses, 3 VCAs e snapshots) com medidores ativos e pílula inteira. |
| 02A-1.png (evidência local) | **Spatializer 3D Deck** (`deck_spatializer` após `btn_tab_spatializer`) | Spotlight ciano sobre plot polar, curva de roll-off e controles `Min/Max Distance`, `Sound Size` e `Min Extent` com pílula sem corte. |

## 2. Validação de Cues e Pós-Produção (`ffmpeg` / `ffprobe`)

- `node scripts/tutorial_studio_cue.js --validate` -> `[validate] Validated 17 cue file(s): OK` (exit 0, `00A`..`06B` e roteiros correspondentes).
- `node scripts/tutorial_studio_cue.js --cue docs/tutorials/playground/cues/03A.json --dry-run` -> exit 0.
- `./scripts/finalize_tutorial_video.ps1 -InputPath "sample.mp4" -WhatIf` -> exit 0.
- Amostra sintética MP4 gerada em `$env:TEMP` (`640x360`, 44100 Hz a `-22 dBFS`), normalizada em 2 passadas com `./scripts/finalize_tutorial_video.ps1` e removida após validação:
  - `ffprobe`: vídeo `h264`, áudio `aac`, `sample_rate: 48000`, `channels: 2`.
  - `ffmpeg loudnorm`: `input_i = "-14.06"`, `input_tp = "-12.87"`.

## 3. Gates Finais

- **Studio (`./scripts/verify.ps1 -Layer studio`):** `RESULT: OK` (`11/11` smokes Release aprovados, incluindo `--smoke-pro-ui` e `--smoke-mcp` com `studio_ui_highlight`, além de `build/Studio_Bundle_Release` recriado).
- **Unity (`./scripts/verify.ps1 -Layer unity`):** `RESULT: OK`:
  - `.NET 8` Standalone (`scripts/unity_test.ps1 -DotnetOnly`): `40/40` aprovados (incluindo `PG_09_CameraMoveCue_SmoothStepUpdatesPoseFootstepsAndZones` e `PG_10_CameraOrbitCue_OrbitsTotem360AndUpdatesBuiltin3DParams`).
  - Unity `6000.0.34f1` `EditMode` (`build/UnityTestResults/results_EditMode.xml`): `5/5` aprovados (compilação limpa de `OpenAudio.Editor.dll` com `PlaygroundTutorialDirectorWindow.cs` e `OpenAudio.Runtime.dll`, zero diff em `unity/OpenAudioUnity/Packages/manifest.json`).
  - Unity `6000.0.34f1` `PlayMode` (`build/UnityTestResults/results_PlayMode.xml`): `38/38` aprovados. Nota: adicionada guarda headless (`SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null || Application.isBatchMode`) em `AudioPlaygroundEnvironmentBuilder.BuildReflectionProbe` para evitar crash nativo de `ReflectionProbeMode.Realtime` em `-batchmode -nographics`, preservando integralmente a renderização PBR/HDR com GPU.
- **Documentação e OpenSpec (`./scripts/verify.ps1 -Layer docs` + `openspec validate playground-video-tutorials --strict`):** aprovados com saída `0` (`openspec validate --all --strict`, integridade de links, e `docs/ai/memory.md` em `91/100` linhas).
- **Proteção de escopo:** `unity/OpenAudioUnity/Packages/manifest.json`, `native/include/oae_audio.h`, `example_project/playground_project.json` e `example_project/playground_vo_project.json` sem alterações; nenhum `.mp4`/`.mkv`/`.mov` versionado no repositório.
