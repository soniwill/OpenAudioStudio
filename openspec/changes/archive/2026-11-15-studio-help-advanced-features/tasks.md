# Tasks: studio-help-advanced-features

## 1. Substituição de Imagens Legadas

- [x] 1.1 Atualizar `docs/manual/chapters/10-unity-api-csharp-e-live.html` substituindo a imagem legada `studio-deck-diagnostics.png` por `deck-tab6-diagnostics.png` e verificar renderização.
- [x] 1.2 Atualizar `docs/manual/chapters/13-unity-timeline-e-bancos.html` substituindo `studio-deck-mixer.png` por `deck-tab1-mixer.png` e verificar renderização.
- [x] 1.3 Atualizar `docs/manual/chapters/14-barramentos-vcas-snapshots.html` substituindo `studio-mixer-controles.png` por `foundations-mixer-deck.png` com legendas atualizadas para o design system e verificar renderização.

## 2. Instrumentos Avançados e Parameter Sheets na Timeline e Inspetor

- [x] 2.1 Expandir `docs/manual/chapters/05-timeline-e-waveforms.html` documentando Command Instruments (`start_event`, `stop_event`, `snapshot_on`, `snapshot_off`), Parameter Sheets (`PSHT`), Async Instruments e Scatterer Instruments na timeline.
- [x] 2.2 Expandir `docs/manual/chapters/06-inspetor-e-modulacao.html` documentando a configuração de Command Instruments no Inspetor, Spawners de Scatterer, Modificadores de Parâmetros (`PMOD`: `velocity` e `hold_during_playback`) e chaves de Programmer Instruments.

## 3. Integração Unity, Espacialização 3D e Built-in Parameters

- [x] 3.1 Atualizar `docs/manual/chapters/08-unity-instalacao-e-workflow.html` e `09-unity-componentes-inspector.html` detalhando os componentes `StudioEventEmitter` e `StudioListener`, seus triggers e modos de parada.
- [x] 3.2 Documentar cones acústicos direcionais, roll-off de HF, `Sound Size`/`Min Extent` e os 5 Built-in Parameters automáticos (`Distance`, `Direction`, `Elevation`, `EventConeAngle`, `Speed`) no Capítulo 9.

## 4. Tutoriais, Glossário e Documentação MCP

- [x] 4.1 Expandir `docs/manual/chapters/11-tutoriais-praticos.html` com tutoriais práticos de (1) evento 3D com atenuação e cones; (2) orquestração com Command Instrument e Snapshots; (3) Parameter Sheet adaptativa.
- [x] 4.2 Atualizar `docs/manual/chapters/12-glossario-e-atalhos.html` incluindo os novos termos e atalhos (<kbd>Ctrl+`</kbd>, <kbd>Shift</kbd>, duplo-clique).
- [x] 4.3 Adicionar documentação das capacidades de automação externa via Servidor MCP no manual do usuário.

## 5. Verificação e Gates de Qualidade

- [x] 5.1 Executar varredura de links e imagens em todos os 18 arquivos HTML do manual garantindo zero links quebrados e zero imagens órfãs faltantes.
- [x] 5.2 Executar o gate `scripts/verify.ps1 -Layer docs` validando que todos os specs atendem ao modo `--strict`.
