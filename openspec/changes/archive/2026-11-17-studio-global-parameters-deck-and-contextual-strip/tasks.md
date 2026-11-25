# Tasks: studio-global-parameters-deck-and-contextual-strip

## 1. Refatoração da TransportBar e Navegação Rápida

- [x] 1.1 Remover `parameterViewport_` e o container de sliders horizontais de `TransportBar`, simplificando `make_layout()`, `resized()` e `preferred_height()`. Verificar compilação limpa da camada Studio.
- [x] 1.2 Adicionar o botão `paramsButton_` com contagem dinâmica de parâmetros globais (`[Params (N)]`) e expor o callback `onOpenGlobalParameters`. Verificar que o clique aciona o callback e que o modo minimizado oculta o botão.

## 2. Fita Retrátil de Parâmetros Contextuais na Timeline

- [x] 2.1 Criar o componente `TimelineParameterStrip` em `TimelinePanel` para exibir exclusivamente parâmetros locais e globais consumidos pelo evento ativo, com `OaeSlider`, caixas de leitura numérica legíveis e reset por duplo-clique.
- [x] 2.2 Implementar recolhimento automático para altura zero quando o evento não consome parâmetros e botão de alternância manual `[v] Parameters (N)`. Verificar comportamento com eventos vazios e com parâmetros vinculados.
- [x] 2.3 Adicionar exibição do rótulo textual nominal para parâmetros do tipo `labeled` na fita contextual. Verificar que o nome do estado ativo é exibido em vez de índice puro.

## 3. Deck de Parâmetros Globais no Bottom Deck

- [x] 3.1 Criar o componente `GlobalParametersDeck` com barra superior de busca textual (`juce::TextEditor`), botões de filtro (`Todos`, `Usados no Evento`, `Modificados`) e ação `Reset All to Default`.
- [x] 3.2 Implementar layout em grid responsivo de cards (2 a 3 colunas) com controles especializados para parâmetros contínuos (fader largo e caixa numérica), discretos e rotulados (botoeira segmentada com texto das opções).
- [x] 3.3 Adicionar telemetria visual de modificadores ativos em tempo real (badge de `velocity` animado, indicador de `seek_speed` e ícone de `hold_during_playback` retido).
- [x] 3.4 Integrar `GlobalParametersDeck` como a aba `DeckTab::Parameters` no `ContextualBottomDeck` e conectar o callback `onOpenGlobalParameters` da `TransportBar` para focar a aba.

## 4. Desacoplamento Flutuante e Persistência do Workspace

- [x] 4.1 Adicionar botão `[Pop-Out ^]` no cabeçalho do `GlobalParametersDeck` integrando a janela flutuante `OaeFloatingWindow` com botão `[Dock Back]` e sem botão de fechar destrutivo do OS.
- [x] 4.2 Garantir compatibilidade da nova aba com a persistência de layouts em `DockingLayoutManager` e presets de fábrica do Studio.

## 5. Documentação e Gates de Validação

- [x] 5.1 Atualizar os capítulos do manual offline (`docs/manual/chapters/01-visao-geral-interface.html`, `03-transporte-e-audicao.html` e `07-deck-inferior-mixer-efeitos.html`) documentando a nova barra de transporte limpa, a fita contextual da Timeline e o Global Parameters Deck.
- [x] 5.2 Executar os gates completos: `scripts/verify.ps1 -Layer studio` (smokes Release) e `scripts/verify.ps1 -Layer docs` (`openspec validate --all --strict`).
