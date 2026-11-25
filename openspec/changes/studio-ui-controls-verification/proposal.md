# Proposal: Verificação Automatizada Ponta a Ponta de Controles de UI do Studio (`studio-ui-controls-verification`)

## Motivação

Controles interativos do OpenAudio Studio (sliders, botões, toggles, checkboxes, combos, campos de texto e widgets customizados) frequentemente deixam de funcionar silenciosamente após evoluções de layout e refatorações. No código atual, três lacunas permitem que regressões passem despercebidas:

1. Os 11 smokes atuais em `studio/Source/Main.cpp` (executados por `scripts/build_studio.ps1`) exercitam apenas recortes isolados em `--smoke-pro-ui`, enquanto `--smoke-mcp` utiliza ferramentas de domínio (`studio_set_mixer_fader`, `studio_move_clip`, `studio_transport`) que **pulam** os widgets reais da interface.
2. Em `studio/Source/MCP/McpServer.cpp`, `studio_trigger_control` invoca `btn->onClick()` diretamente sem verificar `isVisible()`, `isEnabled()`, geometria válida ou `hitTest` real; além disso, diversos controles interativos ainda não possuem `ComponentID` semântico ou compartilham IDs em painéis simultâneos (ex.: `inspector_card_more_actions`), e widgets customizados permanecem opacos na inspeção.
3. Não existe uma trava estrutural automatizada que impeça controles novos de surgirem sem teste nem uma suíte rápida que comprove ponta a ponta que a interação na UI produz tanto a mudança esperada de estado (`AuthoringModel` + `Undo`/`Redo`, `AuditionTransport`, `StudioEngine`, estado de View/Docking) quanto o efeito acústico real no áudio renderizado.

## Escopo

Esta change entrega uma suíte automatizada, rápida e determinística de verificação de controles de UI do OpenAudio Studio, cobrindo cinco frentes integradas:

1. **Inicialização headless e isolamento de ambiente no `MainComponent` (`studio/Source/MainComponent.h/.cpp`)**:
   - Introduzir `MainComponentOptions` (preservando o construtor padrão de desktop) para permitir forçar `StudioEngine::init_headless()`, isolar o caminho do arquivo de layout (`oae_studio_layout.json`) em diretório temporário sem sobrescrever o layout do usuário, desabilitar o auto-load do projeto de exemplo, desabilitar o bind do `LiveBridgeServer` na porta TCP `7450` e controlar os timers a 30 Hz para execução determinística.

2. **Instrumentação mínima de testabilidade na UI (`studio/Source/UI/`, `studio/Source/Testing/UiTestHooks.h`)**:
   - Preencher todos os `ComponentID`s ausentes e desambiguar colisões entre controles visíveis simultaneamente (`AuthoringTreeBrowser`, `TimelinePanel`, `TrackHeaderPanel`, `TimelineTrackLane`, `TimelineParameterStrip`, `LayerStackView`, `MixerDeckPanel`, `PlaylistDeck`, `GlobalParametersDeck`, `DiagnosticsLogPanel`, `SlotHeaderBar` e os 9 cards do `InspectorPanel` + editores de curva), preservando integralmente os IDs documentados para automação MCP.
   - Definir a interface leve `ITestableCustomControl` para os 12 widgets customizados com hit-testing manual (`ClipComponent`, `AutomationLaneComponent`, `TimelineRuler`, `TimelineOverviewMinimap`, `MaskRangeCanvas`, `EffectChainDeck::EqPlot`, `Spatializer3DDeck`, `AhdsrCurveEditor`, `AutomationCurveEditor`, `ParametersListComponent`, `TrackHeaderItem`, `GripHandle`) exporem alvos interativos em coordenadas locais.
   - Definir o seam leve `UiTestDialogSeam` (inativo em produção) para interceptar de forma não-bloqueante os 6 pontos de `FileChooser`/`AlertWindow` e os 4 `PopupMenu::show()` síncronos quando em execução de teste.

3. **Auditor "Zero Controle Órfão" e driver de interação realista (`studio/Source/Testing/`)**:
   - Implementar `UiControlAuditor` para percorrer a hierarquia JUCE real do `MainComponent` e das janelas flutuantes `OaeFloatingPanelWindow` (via `juce::TopLevelWindow`) através de uma matriz canônica de contextos (todas as seleções de entidades e instrumentos, modos `Timeline` e `Layers`, todas as 4 abas do Browser + parâmetros globais, todas as 7 abas do Bottom Deck e ciclo *pop-out*/*dock-back*), falhando deterministicamente diante de qualquer controle interativo sem `ComponentID` único (`MISSING_ID` / `DUPLICATE_ID`), com geometria inválida/recortada (`ZERO_BOUNDS` / `CLIPPED`), ocluído no `hitTest` (`OCCLUDED_HITTEST`) ou sem contrato de teste registrado (`UNCOVERED`).
   - Implementar `UiInteractionDriver` validando obrigatoriamente `isVisible()` na hierarquia, `isEnabled()`, geometria e `hitTest` real antes de sintetizar eventos de mouse, arraste (incluindo `Shift` fino), roda do mouse, teclado, seleção em `ComboBox` e entrada de texto, drenando `MessageManager` entre gestos.

4. **Suíte comportamental e acústica ponta a ponta (`--test-ui-controls`)**:
   - Executar sobre um projeto fixture canônico com WAVs senoidais sintéticos determinísticos compilados em banco de prévia, verificando:
     - **Controles de autoria/modelo**: mutação esperada no `AuthoringModel`, persistência após `refresh()` da UI e reversibilidade completa via `Undo`/`Redo`.
     - **Controles de transporte, mixagem, pista, efeitos DSP, espacialização 3D e parâmetros ao vivo**: mudança em `AuditionTransport`/`StudioEngine` **e** verificação acústica real em buffer PCM headless (`StudioEngine::update()` + `render()` L/R e `get_bus_meter()`), comprovando geração de sinal no Play, silêncio no Stop/Mute, atenuação em dB nos faders, isolamento no Solo, balanço L/R no Pan, resposta espectral em Lowpass/EQ3 e Bypass, atenuação/pan 3D no Spatializer e variação acústica via máscaras/automação nos parâmetros ao vivo.
     - **Controles de visualização, navegação e docking**: alternância de abas, visibilidade de painéis, presets de workspace, zoom/snap, busca, lock do Inspector, colapso de cards e ciclo completo de *pop-out* (`OaeFloatingPanelWindow`) e retorno (`btn_floating_dock_back`).

5. **Execução rápida, filtros e integração ao gate (`studio/Source/Main.cpp`, `studio/CMakeLists.txt`, `scripts/`, `docs/`, `README.md`)**:
   - Expor `--test-ui-controls` com `--filter=<padrao>`, `--audit-only` e `--report=<arquivo.json>` rodando em um único processo headless.
   - Criar `scripts/test_studio_controls.ps1` para reexecução rápida sem rebuild/reempacotamento por padrão e integrar `--test-ui-controls` ao gate `scripts/build_studio.ps1` (`scripts/verify.ps1 -Layer studio` e `-All`).
   - Documentar arquitetura, uso, códigos de erro e manutenção em `docs/testing/studio-ui-controls.md`, com referência no `README.md`.

## Exclusões

- Refatorar o modelo arquitetural de wiring/binding dos controles em `studio/Source/UI/` (migração de lambdas para binding declarativo ou `ApplicationCommandManager` — escopo separado).
- Reescrever o ciclo de `refresh()` / reconstrução dinâmica dos painéis (`InspectorPanel`, `LayerStackView`, `GlobalParametersDeck`).
- Modificar ou expandir ferramentas e contratos do servidor MCP (`mcp-automation` apenas se beneficia passivamente da padronização de `ComponentID`s).
- Exercitar exaustivamente todas as folhas individuais internas de submenus `PopupMenu` (o escopo garante que os acionadores não travem e tenham contrato verificado via `UiTestDialogSeam`, sem testes frágeis amarrados a cada subitem de menu contextual).
- Comparação de screenshots pixel-a-pixel ou testes contra dispositivo físico WASAPI.

## Capabilities

### New Capabilities
*(Nenhuma — os requisitos desta change evoluem capacidades de plataforma já existentes.)*

### Modified Capabilities
- `studio-shell`: Adiciona suporte a inicialização headless/isolada no `MainComponent`, cobertura completa e única de `ComponentID`s + ganchos leves de testabilidade (`ITestableCustomControl`, `UiTestDialogSeam`), auditoria "Zero Controle Órfão", driver de interação realista, cobertura de janelas *pop-out* e suíte `--test-ui-controls` verificando estado + sinal de áudio headless.
- `dev-harness`: Adiciona o script de execução rápida `scripts/test_studio_controls.ps1`, integra `--test-ui-controls` como verificação obrigatória no gate `-Layer studio` (`scripts/build_studio.ps1` / `scripts/verify.ps1`) e exige documentação reproduzível em `docs/testing/studio-ui-controls.md`.

## Impact

- **OpenAudio Studio (`studio/Source/MainComponent.*`, `studio/Source/Main.cpp`, `studio/Source/UI/**`, `studio/Source/Testing/**`, `studio/CMakeLists.txt`)**: Opções em `MainComponent`, preenchimento/desambiguação de `ComponentID`s, interfaces leves `ITestableCustomControl` e `UiTestDialogSeam`, nova pasta `studio/Source/Testing/` com auditor, driver, sonda acústica e contratos por painel, flags CLI em `Main.cpp` e registro explícito dos novos fontes em `studio/CMakeLists.txt`.
- **Scripts e Gates (`scripts/test_studio_controls.ps1`, `scripts/build_studio.ps1`)**: Novo utilitário PowerShell com filtragem rápida e inclusão de `--test-ui-controls` na sequência de smokes do Studio.
- **Documentação (`docs/testing/studio-ui-controls.md`, `README.md`)**: Guia de execução, interpretação de falhas e passo a passo para registrar novos controles no auditor.
- **Camadas `native/` e `unity/`**: Sem alterações.
