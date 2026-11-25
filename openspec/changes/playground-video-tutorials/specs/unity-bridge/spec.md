# Spec Delta — unity-bridge

## Purpose

Define os requisitos da ponte Unity e do kit editorial gratuito para produção de tutoriais em vídeo do Audio Playground, abrangendo o diretor nativo no Editor, movimentação suave de câmera em PlayMode, roteiros de 17 episódios, cues sincronizados e pós-processamento com ffmpeg.

## ADDED Requirements

### Requirement: Diretor de tutorial nativo no Unity Editor (`PlaygroundTutorialDirectorWindow`)
O pacote Unity (`com.openaudio.core`) SHALL expor exclusivamente em código Editor (`OpenAudio/Tutorial Director`, sem novos pacotes em `manifest.json`) a janela `PlaygroundTutorialDirectorWindow` para carregar arquivos de cues JSON dos episódios, navegar entre passos (`Anterior`, `Próximo`, `Parar Destaque`), selecionar o GameObject alvo e destacar campos do `Inspector` via `UnityEditor.Highlighter`.

#### Scenario: Destaque de campo no Inspector durante passo EditMode
- **WHEN** o usuário aciona um passo EditMode de um episódio no `PlaygroundTutorialDirectorWindow`
- **THEN** a janela seleciona o `GameObject` indicado na hierarquia, foca o `Inspector`, invoca `UnityEditor.Highlighter.Highlight("Inspector", <label>)` no rótulo exato do campo serializado e executa `Highlighter.Stop()` ao trocar de passo ou encerrar o destaque

#### Scenario: Isolamento de compilação `.NET 8` e manifesto UPM limpo
- **WHEN** a suíte standalone `.NET 8` (`scripts/unity_test.ps1 -DotnetOnly`) é compilada e `unity/OpenAudioUnity/Packages/manifest.json` é inspecionado
- **THEN** nenhum script fora de `Editor/` referencia `UnityEditor` e o `manifest.json` permanece sem `com.unity.learn.iet-framework` ou `com.unity.recorder`

### Requirement: Movimentação e câmera suaves por estação para gravação em PlayMode do Audio Playground
O controlador `AudioPlaygroundController` SHALL fornecer API programática de câmera com suporte dual (`#if UNITY_5_3_OR_NEWER` e `.NET 8` headless) composta por `BeginCameraMoveCue`, `BeginCameraOrbitCue`, `CancelCameraCue` e `IsCameraCueActive`, permitindo deslocamentos suaves entre o Spawn e as Estações 1–5 e órbita de 360° em torno de `Station2_Emitter3DTotem` com atualização ativa do `StudioListener`.

#### Scenario: Deslocamento suave por estação preservando passos e triggers de zona
- **WHEN** `BeginCameraMoveCue` é acionado em PlayMode entre o Spawn e uma estação do Audio Playground
- **THEN** `m_PlayerTransform` interpola posição e orientação com curva SmoothStep mantendo disparos automáticos de passo em `Wood`, `Concrete` e `Grass` e triggers de zona em funcionamento
- **AND** o cue encerra suavemente ao concluir o trajeto ou é interrompido imediatamente se o usuário pressionar entrada manual de movimento

#### Scenario: Órbita de 360° ao redor do totem 3D atualizando Built-in Parameters
- **WHEN** `BeginCameraOrbitCue` é executado ao redor de `Station2_Emitter3DTotem` (`(0.0, 1.2, 8.0)`)
- **THEN** o `StudioListener` executa órbita contínua voltada ao totem atualizando em tempo real `EventConeAngle`, `Direction`, `Distance`, `Elevation` e `Speed`

### Requirement: Kit de roteiros, cues e pós-processamento gratuito dos 17 episódios do Audio Playground
O repositório SHALL incluir em `docs/tutorials/playground/` o guia mestre gratuito (OpenScreen/OBS + WASAPI), os 17 roteiros de cena em 3 atos (`00A`–`06B`) cobrindo as 5 estações no Studio (`Timeline` e `Layers`), Unity e técnicas de áudio (`Multi Instrument` anti-metralhadora e Ducking por Sidechain/Snapshot), os cues JSON correspondentes, `scripts/tutorial_studio_cue.js` e `scripts/finalize_tutorial_video.ps1`.

#### Scenario: Cobertura completa e validação estrutural dos 17 episódios e cues
- **WHEN** os 17 arquivos de roteiro e seus JSONs de cues em `docs/tutorials/playground/` são validados via `node scripts/tutorial_studio_cue.js --validate`
- **THEN** todos os episódios `00A` a `06B` existem e validam contra o esquema comum referenciando apenas `component_id`s do Studio, eventos/snapshots do Playground e rótulos existentes do Inspector Unity

#### Scenario: Normalização gratuita para YouTube e acabamento com `ffmpeg`
- **WHEN** `scripts/finalize_tutorial_video.ps1` é executado sobre uma gravação bruta do Studio ou da Unity
- **THEN** invoca `ffmpeg` em duas passadas para normalizar o áudio para `-14 LUFS` (`TP -1.0 dBTP`, 48 kHz AAC), aceitando parâmetros opcionais `-Start`, `-End`, `-Subtitles` e `-WhatIf`
