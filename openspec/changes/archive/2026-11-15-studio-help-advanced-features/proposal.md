# Proposal: studio-help-advanced-features

## Why

Apesar dos recentes avanços no manual offline cobrindo a teoria de fundamentos e o Bottom Deck, o manual ainda apresenta lacunas técnicas materiais e três imagens legadas de 12/11 em capítulos de Unity e Mixagem. Capacidades já consolidadas no motor e no Studio — como **Command Instruments**, **Scatterer Instruments**, **Async Instruments**, **Parameter Sheets (`PSHT`)**, **Modificadores de Parâmetros (`PMOD`)**, os componentes Unity **`StudioEventEmitter`** e **`StudioListener`** com espacialização 3D física e **Built-in Parameters automáticos** — não estão documentadas para os usuários e designers de som. Esta mudança atualiza o manual offline para paridade total com o estado atual do produto.

## What Changes

- **Substituição de Imagens Legadas**:
  - Atualizar [10-unity-api-csharp-e-live.html](../../docs/manual/chapters/10-unity-api-csharp-e-live.html) trocando a imagem legada `studio-deck-diagnostics.png` pela captura moderna `deck-tab6-diagnostics.png`.
  - Atualizar [13-unity-timeline-e-bancos.html](../../docs/manual/chapters/13-unity-timeline-e-bancos.html) trocando `studio-deck-mixer.png` por `deck-tab1-mixer.png`.
  - Atualizar [14-barramentos-vcas-snapshots.html](../../docs/manual/chapters/14-barramentos-vcas-snapshots.html) trocando `studio-mixer-controles.png` pela captura real `foundations-mixer-deck.png` com legendas atualizadas para o design system atual.
- **Instrumentos Avançados e Parameter Sheets nos Capítulos 5 e 6**:
  - Documentar **Command Instruments** (disparo/parada de eventos `start_event`/`stop_event` e ativação/desativação de snapshots `snapshot_on`/`snapshot_off` com alvos e tempos).
  - Documentar **Scatterer Instruments** (parâmetros de spawner `Spawn Interval`, `Polyphony`, `Total Sounds`, `Distance Min/Max`, `3D Spread` e playlist estocástica).
  - Documentar **Async Instruments** (streaming contínuo de áudio longo e trilhas musicais direto do disco sem bloqueio de I/O).
  - Documentar **Programmer Instruments** (chaves da `audio_table`, diálogo de localização e substituição dinâmica via API).
  - Documentar **Parameter Sheets (`PSHT`)** (linhas de tempo orientadas por parâmetros físicos como RPM, velocidade e distância em vez de tempo linear em segundos).
  - Documentar **Modificadores de Parâmetros (`PMOD`)** (`velocity` para sensibilidade dinâmica e `hold_during_playback` para prender o valor do parâmetro enquanto a voz toca).
- **Integração Unity e Espacialização 3D nos Capítulos 8 e 9**:
  - Documentar **`StudioEventEmitter`** e **`StudioListener`** com seus ciclos de vida, triggers de disparo e modos de parada (`Immediate` vs `AllowFadeout`).
  - Documentar a simulação acústica 3D: atenuação por distância, cones direcionais de emissão (ângulos interno e externo e atenuação externa) e roll-off de altas frequências.
  - Documentar o envelopment acústico: `Sound Size` e `Min Extent` quando o ouvinte entra na área de dispersão da fonte sonora.
  - Documentar os 5 **Built-in 3D Parameters automáticos** calculados pelo motor em tempo real: `Distance`, `Direction`, `Elevation`, `EventConeAngle` e `Speed`.
- **Tutoriais e Glossário nos Capítulos 11 e 12**:
  - Adicionar tutoriais práticos passo a passo cobrindo: (1) criação de evento 3D com atenuação e cones; (2) orquestração com Command Instrument e Snapshots; (3) criação de Parameter Sheet adaptativa.
  - Atualizar o Glossário e atalhos com novos termos (Command, Scatterer, Async, Parameter Sheet, PMOD, Breadcrumbs, Swatches) e o atalho <kbd>Ctrl+`</kbd> (Hide All / Restore).
- **Documentação do Servidor MCP do Studio**:
  - Adicionar uma seção técnica no Capítulo 1 ou apêndice sobre o servidor MCP integrado do Studio, permitindo automação por agentes de IA e scripts externos.

## Capabilities

### Modified Capabilities
- `studio-shell`: Atualizar o spec canônico com o requisito formal de cobertura no manual offline para instrumentos avançados (Command, Scatterer, Async, Programmer), Parameter Sheets, Modificadores, recursos avançados de Unity/3D e substituição das imagens legadas.

## Impact

- **Documentação**: Atualização de 7 capítulos HTML (`05`, `06`, `08`, `09`, `10`, `11`, `12`, `13`, `14`), substituição de referências de imagens e verificação do índice de busca em `manual.js`.
- **Código**: Nenhuma alteração no código C++, C# ou binários do runtime (mudança puramente documental e instrucional).
- **Verificação**: `scripts/verify.ps1 -Layer docs` e validação estrita do OpenSpec.
