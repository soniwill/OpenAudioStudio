# Proposal: studio-help-bottom-deck

## Why

O Bottom Deck do OpenAudio Studio abriga as seis ferramentas contextuais mais vitais para a autoria sonora: Mixer, Effects Rack, Playlist/Weights, Logic & Triggers, 3D Spatializer e Diagnostics & Log. O manual atual (`07-deck-inferior-mixer-efeitos.html`) documenta esse painel de forma resumida e essencialmente mecânica, faltando profundidade sobre a teoria acústica de cada DSP, a matemática de balística e K-System no mixer, a inteligência estocástica de multi-amostras, a não-linearidade adaptativa da timeline e a física psicoacústica de atenuação e dispersão em 3D.

Esta change expande e aprofunda exaustivamente o manual do Bottom Deck, transformando o Capítulo 7 no guia de referência técnica e de engenharia das 6 abas, ilustrado por capturas individuais em alta fidelidade geradas via MCP (`studio_ui_screenshot` com `studio_trigger_control`) e diagramas estruturais ASCII.

## What Changes

- **Aprofundamento Teórico e Prático das 6 Abas do Bottom Deck em `docs/manual/chapters/07-deck-inferior-mixer-efeitos.html`:**
  - **Aba 1 (Mixer Deck):** Teoria de tiras de canal, controle de faders, Mute/Solo, medição de pico vs. RMS vs. escalas K (K-12, K-14, K-20) e EBU R128, margem de headroom e auditioning de Snapshots.
  - **Aba 2 (Effects Rack):** Teoria de encadeamento de inserção (Insert Chain), dissecação técnica dos 7 DSPs (EQ3, Compressor, Limiter, Reverb, Delay, Chorus, Distortion) com parâmetros, faixas operacionais e recomendações acústicas.
  - **Aba 3 (Playlist / Weights):** Psicoacústica da repetição e prevenção do efeito metralhadora, modos de seleção (Random, Shuffle anti-repetição e Sequential) e pesos de probabilidade com distribuição estatística.
  - **Aba 4 (Logic & Triggers):** Linha do tempo adaptativa não-linear, Loop Regions, Transition Markers condicionados a parâmetros, quantização musical rítmica (Bars e Beats) e Sustain Points aguardando KeyOff.
  - **Aba 5 (3D Spatializer):** Psicoacústica espacial tridimensional, atenuação por distância (curvas Logarítmica, Linear e Inverse Square), cones direcionais de dispersão, absorção atmosférica de agudos (HF Roll-off) e envelopment/tamanho da fonte (`Sound Size`).
  - **Aba 6 (Diagnostics & Log):** Telemetria em tempo real, orçamentos de vozes ativas vs. virtuais, validação de integridade CRC32 na compilação de bancos e diagnóstico de assets ausentes.
- **Evidências Visuais Automatizadas via MCP:**
  - Geração de 6 capturas focadas em cada aba correspondente (`deck-tab1-mixer.png`, `deck-tab2-effects.png`, `deck-tab3-playlist.png`, `deck-tab4-logic.png`, `deck-tab5-spatializer.png`, `deck-tab6-diagnostics.png`).
- **Navegação e Referência:**
  - Atualização do índice interno do capítulo (`page-toc`) e do índice geral do manual.

## Capabilities

### New Capabilities
*(Nenhuma nova capacidade de plataforma ou conceito introduzida).*

### Modified Capabilities
- `studio-shell`: Atualiza os requisitos de documentação integrada e navegação do Studio com a especificação canônica de cobertura profunda e exemplos de uso para as seis abas contextuais do Bottom Deck.

## Impact

- **Documentação e Ativos:** Expansão de `docs/manual/chapters/07-deck-inferior-mixer-efeitos.html` e adição de 6 imagens PNG em `docs/manual/assets/img/`.
- **Camadas do Repositório:** Afeta exclusivamente a camada `docs` (`scripts/verify.ps1 -Layer docs`).
- **Código e Binários:** Sem modificações em código C++, C# ou nos motores compilados.
