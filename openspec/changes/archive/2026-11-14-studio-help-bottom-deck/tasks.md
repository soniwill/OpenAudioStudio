# Tasks: studio-help-bottom-deck

## 1. Captura Visual das 6 Abas via MCP

- [x] 1.1 Comutar programaticamente entre as seis abas do Bottom Deck (`btn_tab_mixer`, `btn_tab_effects`, `btn_tab_playlist`, `btn_tab_logic`, `btn_tab_spatializer`, `btn_tab_diagnostics`) usando `studio_trigger_control` e capturar a área do deck via `studio_ui_screenshot`, gerando os arquivos PNG correspondentes em `docs/manual/assets/img/` e validando a existência e legibilidade das imagens.

## 2. Redação Teórica: Mixer Deck e Effects Rack (Abas 1 e 2)

- [x] 2.1 Expandir as seções do Mixer Deck e Effects Rack em `docs/manual/chapters/07-deck-inferior-mixer-efeitos.html`, cobrindo channel strips, balística de medição (Peak, RMS, K-12/14/20), margem de headroom, auditioning de snapshots e a teoria acústica detalhada com parâmetros e faixas dos 7 DSPs nativos (EQ3, Compressor, Limiter, Reverb, Delay, Chorus e Distortion), verificando a formatação semântica e os links internos.

## 3. Redação Teórica: Playlist/Weights e Logic & Triggers (Abas 3 e 4)

- [x] 3.1 Expandir as seções de Playlist/Weights e Logic & Triggers em `docs/manual/chapters/07-deck-inferior-mixer-efeitos.html`, cobrindo a psicoacústica anti-metralhadora, modos Random, Shuffle e Sequential, pesos estatísticos, loop regions, marcadores de transição com parâmetros, quantização rítmica em compassos/batidas e sustain points com KeyOff, verificando os diagramas ASCII de fluxo temporal.

## 4. Redação Teórica: 3D Spatializer e Diagnostics & Log (Abas 5 e 6)

- [x] 4.1 Expandir as seções de 3D Spatializer e Diagnostics & Log em `docs/manual/chapters/07-deck-inferior-mixer-efeitos.html`, cobrindo a física de atenuação por distância, cones de diretividade, absorção atmosférica de agudos (HF roll-off), envelopment/sound size, além de telemetria de vozes ativas/virtuais e relatórios de integridade de bancos e assets, validando a integridade das âncoras e navegação da página.

## 5. Validação e Gates de Documentação

- [x] 5.1 Executar a validação estrita dos gates de documentação através de `scripts/verify.ps1 -Layer docs` (cobrindo `openspec validate --all --strict`, verificação de links markdown e conferência do limite de 100 linhas em `docs/ai/memory.md`), garantindo aprovação com código de saída zero.
