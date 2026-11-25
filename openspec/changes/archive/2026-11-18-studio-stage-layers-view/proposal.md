# Proposal

## Why

A interface atual de linha do tempo do Studio segue a metáfora linear de DAW tradicional (réguas temporais, pistas horizontais e curvas de automação separadas), o que cria atrito cognitivo para desenvolvedores de jogos e compositores que trabalham com **música adaptativa baseada em stems** e **ambiências em camadas**. Nesses casos de uso, o eixo decisivo não é a posição temporal absoluta, mas sim quais camadas estão ativas conforme os parâmetros de estado do jogo.

Introduzir um modo alternativo de visualização em pilha de camadas (**Layers**, inspirado na organização visual do Photoshop combinada ao `InspectorPanel` estilo Unity já existente no Studio) democratiza a autoria de áudio adaptativo sem substituir, alterar ou remover a interface de Timeline clássica.

## What Changes

- **Seletor de modo no cabeçalho do palco (`TimelinePanel`)**: Adiciona um controle segmentado `[ Timeline | Layers ]` na barra superior do `TimelinePanel`, preservando `Timeline` como modo padrão e não alterando o `DockingLayoutManager` nem o arquivo `oae_studio_layout.json`.
- **Nova visão `LayerStackView` (`studio/Source/UI/LayerStackView.h/.cpp`)**: Exibida no palco central quando o modo `Layers` estiver selecionado, mantendo a `TimelineParameterStrip` ativa no topo para simulação em tempo real.
- **Cards horizontais bipartidos de stems (`StemLayerCard`)**:
  - **Coluna esquerda (`Stem & Mix`)**: Faixa de cor da pista, botão de visibilidade (Olho mapeado para Mute), botão Solo (`S`), nome do clipe principal, badge de sincronismo/quantização, miniatura poligonal da waveform via `WaveformCache` e controle de Opacidade (`0%–100%` mapeado para `track.volume_db`).
  - **Coluna direita (`Máscara de Ativação`)**: Seletor de parâmetro associado, rampa de *fade-in*, barra visual da faixa ativa `[min .. max]` na escala normalizada `0.0 .. 1.0`, badge de status em tempo real e linha vertical do cursor do parâmetro ao vivo alinhada entre todas as camadas.
- **Camadas de ajuste / filtros de estado (`Adjustment Layer`)**: Representação visual no topo da pilha para efeitos/snapshots globais condicionados por parâmetro (ex.: filtro Low-Pass por limiar de parâmetro), com indicação imediata de estado ativo/inativo.
- **Tradução bidirecional não-destrutiva e round-trip**: A Máscara de Ativação lê e gera/atualiza uma `AutomationCurve` de `volume` na `EventTrack` no `AuthoringModel` com passos atômicos de Undo/Redo, garantindo compatibilidade total ao alternar entre `Layers` e `Timeline`.
- **Criação rápida e drag-and-drop de stems**: Botão `+ Stem Layer` e suporte a arrastar arquivos de áudio válidos (`.wav`, `.ogg`, `.mp3`, `.flac`) sobre a `LayerStackView`, importando o asset e criando uma faixa com `Single Instrument` configurado com `loop = true`.
- **Exclusões (Out of Scope)**:
  - Nenhuma alteração no formato binário `.bank`, no compilador de bancos, na ABI C (`native/include/oae/`) ou no bridge Unity.
  - Nenhuma modificação de layout ou comportamento padrão no modo `Timeline`, no `DockingLayoutManager` (`DockPanelId`) ou no `InspectorPanel` (exceto a sincronização padrão via `select_track`/`select_clip`).

## Capabilities

### New Capabilities

_(Nenhuma — a mudança estende a interface de autoria do aplicativo desktop existente.)_

### Modified Capabilities

- `studio-shell`: Adiciona o seletor de visualização `Timeline | Layers` no `TimelinePanel`, a visão `LayerStackView` com cards bipartidos de stems e ajuste, máscara visual de parâmetro sincronizada com `AutomationCurve` e cursor ao vivo, além da criação e importação por drag-and-drop de stems em loop.

## Impact

- **Camada Studio UI (`studio/Source/UI/`)**: Novos arquivos `LayerStackView.h` / `LayerStackView.cpp` e integração não-invasiva em `TimelinePanel.h` / `TimelinePanel.cpp` (e `studio/CMakeLists.txt`).
- **Modelo de Autoria (`studio/Source/Model/AuthoringModel.*`)**: Reutilização dos comandos existentes (`set_track_mute`, `set_track_solo`, `set_track_volume`, `add_instrument`, automação de faixa) ou helper dedicado de atualização atômica de curva de máscara com Undo/Redo.
- **Referência Visual**: Protótipo interativo validado no Figma (`OpenAudioStudio — Stage Layers View`, `https://www.figma.com/design/HvAJmcrKKJY45OpBA83D4A`).
- **Verificação e Documentação**: Adição de smoke test dedicado no executável do Studio, verificação ao vivo via MCP (`studio_ui_screenshot`) e atualização do manual offline (`studio/Resources/help/`).
