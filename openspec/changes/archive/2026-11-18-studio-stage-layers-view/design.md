# Design

## Context

Veja `proposal.md` para a motivação de produto e os benefícios de fluxo de trabalho para stems adaptativos. No código atual do Studio (`studio/Source/UI/`), três componentes já sustentam a experiência desejada sem exigir reescrita estrutural:

- `TimelinePanel` já hospeda a `TimelineParameterStrip` retrátil para controle ao vivo de parâmetros no topo do palco e mantém o cache de waveforms (`WaveformCache`).
- `InspectorPanel` já está modularizado em cards colapsáveis (`EventHeaderCard`, `TrackCard`, `ClipCard`) com bloqueio de seleção (*Lock Inspector*) e histórico atômico via `AuthoringModel`.
- `DockingLayoutManager` gerencia um conjunto fixo de painéis (`DockPanelId`, incluindo `DockPanelId::Timeline` como palco central) persistido em `oae_studio_layout.json`.

A referência visual completa validada para esta interface está documentada no Figma em:
`https://www.figma.com/design/HvAJmcrKKJY45OpBA83D4A` (*OpenAudioStudio — Stage Layers View*).

## Goals / Non-Goals

**Goals:**
- Introduzir a visão alternativa `Layers` no palco central mantendo 100% da `Timeline` atual intocada por padrão.
- Reutilizar integralmente `AuthoringModel`, `ProjectDocument`, `TimelineParameterStrip`, `InspectorPanel`, `WaveformCache` e `AuditionTransport`.
- Garantir tradução determinística e reversível (*round-trip*) entre Máscaras Visuais de camada e `AutomationCurve`s de `volume` nas `EventTrack`s, preservando Undo/Redo atômico.
- Alinhar horizontalmente as barras de máscara (`0.0 .. 1.0`) e o cursor ao vivo entre todas as camadas da pilha para leitura imediata da coreografia sonora do evento.

**Non-Goals:**
- Não criar novos tipos de nó ou propriedades no formato binário `.bank`, no JSON compilado de banco ou na ABI C.
- Não adicionar um novo `DockPanelId` no `DockingLayoutManager` nem alterar o schema de `oae_studio_layout.json`.
- Não sobrescrever curvas de automação complexas editadas manualmente na `Timeline`.
- Não alterar a barra, o inspetor nem o comportamento do modo `Timeline` quando ativo.

## Decisions

### 1. Sub-modo interno no `TimelinePanel` em vez de novo painel no `DockingLayoutManager`

- **Decisão**: Modelar `StageViewMode { Timeline, Layers }` dentro do próprio `TimelinePanel`, alternado por um seletor segmentado `[ Timeline | Layers ]` na barra superior do palco.
- **Alternativa considerada**: Criar um `DockPanelId::Layers` no `DockingLayoutManager` ou uma nova aba no `ContextualBottomDeck`.
- **Justificativa**: Alterar `dockPanelCount` impactaria `oae_studio_layout.json`, os presets de workspace e os 11 testes de smoke existentes. Mantendo `LayerStackView` como filho alternável dentro de `TimelinePanel` (com `setVisible(false)` no modo padrão `Timeline`), o modo clássico e todos os `ComponentID`s atuais permanecem 100% isolados, e a `TimelineParameterStrip` é compartilhada naturalmente no topo dos dois modos.

### 2. Arquitetura de componentes e layout bipartido da `LayerStackView`

```text
+-------------------------------------------------------------------------+
| TimelinePanel (DockPanelId::Timeline)                                   |
| [ < Breadcrumb ]     [ (x) Timeline | ( ) Layers ]    [ + Stem ] [Zoom] |
+-------------------------------------------------------------------------+
| TimelineParameterStrip (compartilhada nos dois modos)                   |
+-----------------------------------+-------------------------------------+
| Modo Timeline (default)           | Modo Layers (novo)                  |
|                                   |                                     |
| - TimelineOverviewMinimap         | + LayerStackView (Viewport)         |
| - TimelineRuler                   |   - ColumnHeader (350px | 0.0..1.0) |
| - TrackHeaderPanel                |   - AdjustmentLayerCard (opcional)  |
| - TimelineTrackLane               |   - StemLayerCard[] (bipartido):    |
|                                   |     * Esq (350px): Cor, Olho, Solo, |
|                                   |       Nome, Sync, Waveform, Opac.%  |
|                                   |     * Dir (FILL): Param, Fade,      |
|                                   |       MaskRangeCanvas + Cursor vivo |
+-----------------------------------+-------------------------------------+
```

- **Decisão**: Cada `StemLayerCard` possui coluna esquerda com largura fixa (`350 px`) para identidade, miniatura de waveform (`WaveformCache`) e fader de opacidade (`0%–100%` -> `track.volume_db`), separada por divisor de `1 px` da coluna direita expansível (`MaskRangeCanvas`).
- **Justificativa**: Com a coluna esquerda fixa e todas as barras de máscara ocupando exatamente a mesma largura útil à direita, o intervalo normalizado `0.0 .. 1.0` e a linha vertical do cursor do parâmetro ao vivo ficam alinhados pixel a pixel de cima a baixo em todos os cards, permitindo comparar onde cada stem entra e sai de relance (conforme validado no protótipo do Figma).

### 3. Mapeamento bidirecional não-destrutivo entre Máscara Visual e `AutomationCurve`

- **Decisão**: Representar a máscara da camada sem estender o schema do banco compilado, sintetizando e reconhecendo uma `AutomationCurve` da faixa com `target_property == "volume"` vinculada ao `param_name` selecionado:
  - Quando a máscara tem faixa `[min, max]` e suavização `fade` (`fade >= 0`), o helper gera de 2 a 4 pontos normalizados na curva de volume em dB (de `-80.0 dB` fora da máscara até `0.0 dB` dentro de `[min, max]`, com rampa suave nas bordas `min - fade` e `max + fade`, respeitando `[0.0, 1.0]`).
  - Ao carregar uma faixa na `LayerStackView`:
    - Se não houver curva de volume por parâmetro: exibe `Sempre Ativo` (`[0.0, 1.0]`).
    - Se houver exatamente uma curva de volume reconhecível como janela monotônica/banda de 2–4 pontos: extrai `param_name`, `min`, `max` e `fade` para renderizar na barra interativa.
    - Se houver múltiplas curvas de volume ou mais de 4 pontos com formato arbitrário: exibe badge `"Curva Personalizada"` na coluna direita com botão `"Editar na Timeline"`, bloqueando edição pela barra simplificada para evitar perda de dados.
- **Alternativa considerada**: Criar um novo campo `layer_mask` em `EventTrack` no `ProjectDocument`.
- **Justificativa**: Traduzir diretamente para `AutomationCurve` garante compatibilidade automática com o motor C++, compilação `.bank`, Live-Linking e inspeção no `TimelinePanel` clássico sem precisar migrar projetos antigos.

### 4. Mapeamento de Opacidade (`0%–100%`), Visibilidade (Olho) e Ajuste (Adjustment Layer)

- **Opacidade**: Mapeia linearmente `0%` a `100%` (ou escala perceptual suave: `0%` = `-80 dB`, `100%` = `0 dB` via curva cúbica/logarítmica padrão de áudio) para `AuthoringModel::set_track_volume(event_path, track_index, volume_db)`.
- **Olho / Solo**: Mapeia diretamente para `AuthoringModel::set_track_mute(...)` e `set_track_solo(...)`.
- **Camada de Ajuste (`Adjustment Layer`)**: Representada no topo da pilha quando o evento possui efeitos de bus/faixa com modulação ou automação por parâmetro de limiar (ex.: filtro Low-Pass no evento controlado por `$vida_baixa`), permitindo abrir o respectivo efeito no `InspectorPanel` / `EffectChainDeck` ao clicar.

### 5. Drag-and-Drop e Criação de Stem em Loop

- Reutilizar o pipeline existente de importação de assets (`AudioFormatManager` / cópia para `assets/` + cadastro de `SampleDef` no `AuthoringModel`) implementado na change `studio-audio-formats-and-drag-drop`, configurando o `Single Instrument` criado com `loop = true` para que stems recém-arrastados já entrem sincronizados em repetição contínua.

## Risks / Trade-offs

- **[Risco] Curva de automação manual preexistente ser interpretada incorretamente como máscara** -> **Mitigação**: Validar estritamente a assinatura de até 4 pontos (rampa de subida/platô/descida) ao ler a curva da faixa; qualquer curva fora desse padrão exibe `"Curva Personalizada"` somente leitura na `LayerStackView` com atalho direto para a `Timeline`.
- **[Risco] Desempenho de repintura ao arrastar sliders de parâmetros ao vivo** -> **Mitigação**: Em modo `Layers`, `LayerStackView` escuta mudanças de valor de parâmetro em `AuditionTransport` / `AuthoringModel` usando `repaint()` apenas na sub-região dos `MaskRangeCanvas` e labels de status, sem reconstruir os componentes dos cards nem regenerar thumbnails do `WaveformCache`.
- **[Risco] Regressão visual nos smokes do `TimelinePanel`** -> **Mitigação**: Manter `StageViewMode::Timeline` como inicialização padrão no construtor e preservar todos os `ComponentID`s e bounds existentes intactos quando `Layers` está inativo.
