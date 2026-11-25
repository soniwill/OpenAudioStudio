# Verificação Headless de Controles da UI do OpenAudio Studio

A suíte `--test-ui-controls` verifica de forma determinística e sem dispositivo físico de áudio (**headless**) que 100% dos controles interativos do **OpenAudio Studio** possuem identidade estável (`ComponentID`), geometria e *hit-test* válidos, e cumprem seus contratos comportamentais (modelo, *Undo/Redo* e UI) e acústicos (PCM via `StudioEngine::render()`).

---

## 1. Arquitetura

A verificação é composta por quatro subsistemas em `studio/Source/Testing/`:

1. **`UiInteractionDriver` (`UiInteractionDriver.h/.cpp`)**:
   - Despacha interações sintéticas realistas (`click`, `double_click`, `drag`, `drag_slider`, `wheel`, `key_press`, `select_combo_item`, `type_text`) e alvos nomeados de widgets customizados (`click_target`, `double_click_target`, `drag_target`, `wheel_target`).
   - Executa um **pre-flight físico obrigatório** antes de qualquer evento:
     - `isShowing()` / cadeia de `isVisible()` até a raiz (`MainComponent` ou `OaeFloatingPanelWindow`);
     - `isEnabled()`;
     - `getLocalBounds()` e interseção visível positiva após *auto-scroll* em `juce::Viewport` ancestrais;
     - *Hit-test* real via `root->getComponentAt(ptInRoot)` garantindo que o controle (ou subcomponente folha legítimo) não esteja ocluído por outro componente irmão ou *overlay*.
   - Inclui um **autoteste negativo embutido** (`run_preflight_negative_self_test`) que prova que controles invisíveis, desabilitados ou cobertos por bloqueadores falham no *pre-flight* sem disparar `onClick`.

2. **`UiControlAuditor` (`UiControlAuditor.h/.cpp`)**:
   - Percorre `MainComponent` e todas as janelas flutuantes (`juce::TopLevelWindow`) nos **7 contextos canônicos** da aplicação (layout padrão com os 7 *Bottom Decks*, modo *Layers*, seleção dos 6 tipos de instrumentos de clipe, seleção de `Bus`/`VCA`/`GlobalParameter`/`Snapshot`/`Sample`, *menus/popups* via `UiTestDialogSeam`, e painéis destacados em janela flutuante).
   - Aplica a invariante **"Zero Controles Órfãos"**: todo controle interativo visível deve possuir `ComponentID` único no contexto ativo, geometria positiva, *targets* customizados alcançáveis e cobertura declarada em pelo menos um contrato (`UiControlContract`).

3. **`UiControlTestFixture` & `HeadlessAudioProbe` (`UiControlTestFixture.h/.cpp`, `HeadlessAudioProbe.h/.cpp`)**:
   - Gera em diretório temporário isolado três arquivos WAV sintéticos (`440 Hz`, `880 Hz` e dual-tone `220 Hz + 4000 Hz`) e um `project.json` canônico compilado uma única vez via `PreviewRebuilder::rebuild_synchronously()`.
   - Restaura mutações entre contratos em memória (`restore_golden_model()`) sem recompilar o banco quando a revisão de conteúdo permanece inalterada.
   - `HeadlessAudioProbe::capture()` bombeia `StudioEngine::update()` + `StudioEngine::render()` em blocos de 512 frames a 48 kHz e calcula RMS L/R, dBFS, razão estéreo `lrRatio`, energia espectral Goertzel em `220 Hz`, `440 Hz`, `880 Hz` e `4000 Hz`, além de telemetria de medidor (`get_bus_meter()`).

4. **`UiTestDialogSeam` & `ITestableCustomControl` (`UiTestHooks.h`)**:
   - `ITestableCustomControl` expõe sub-alvos geométricos nomeados (`TestableTarget`) para os 12 widgets customizados (`ClipComponent`, `AutomationLaneComponent`, `TimelineRuler`, `TimelineOverviewMinimap`, `MaskRangeCanvas`, `EffectChainDeck::EqPlot`, `Spatializer3DDeck`, `AhdsrCurveEditor`, `AutomationCurveEditor`, `ParametersListComponent`, `TrackHeaderItem` e `GripHandle`).
   - `UiTestDialogSeam` intercepta `FileChooser`, `AlertWindow`, `PopupMenu` e abertura de navegador externo durante `--test-ui-controls` sem bloquear a thread de mensagens.

---

## 2. Execução via CLI e PowerShell

Execute a suíte completa ou filtrada pelo script PowerShell:

```powershell
# Suíte completa (auditoria dos 7 contextos + todos os contratos comportamentais e acústicos)
./scripts/test_studio_controls.ps1

# Filtrar por padrão glob de contrato ou ComponentID (ciclo rápido < 1s)
./scripts/test_studio_controls.ps1 -Filter "mixer*"
./scripts/test_studio_controls.ps1 -Filter "decks.spatializer_3d"

# Executar apenas a auditoria estrutural e geométrica dos 7 contextos
./scripts/test_studio_controls.ps1 -AuditOnly

# Exportar relatório JSON detalhado
./scripts/test_studio_controls.ps1 -ReportPath "build/ui_controls_report.json"

# Recompilar oae_studio antes de rodar
./scripts/test_studio_controls.ps1 -Config Release -Build
```

Ou diretamente no executável do Studio:

```powershell
"./build/Studio_Release/studio/oae_studio_artefacts/Release/OpenAudio Studio.exe" --test-ui-controls --filter=inspector* --report=build/report.json
```

A suíte também roda automaticamente dentro de `./scripts/build_studio.ps1` e `./scripts/verify.ps1 -Layer studio`.

---

## 3. Códigos de Diagnóstico

| Código | Origem | Significado |
| --- | --- | --- |
| `MISSING_ID` | Auditor | Controle interativo visível com `ComponentID` vazio. |
| `DUPLICATE_ID` | Auditor | Dois ou mais controles visíveis simultaneamente no mesmo contexto compartilham o mesmo `ComponentID`. |
| `ZERO_BOUNDS` | Auditor / Pre-flight | Controle ou `TestableTarget` com largura ou altura `<= 0`, ou widget customizado sem *targets*. |
| `CLIPPED` | Auditor / Pre-flight | Controle fora da área visível de seu `Viewport` ou janela raiz mesmo após *auto-scroll*. |
| `OCCLUDED_HITTEST` | Auditor / Pre-flight | `root->getComponentAt(point)` atingiu outro componente sobreposto em vez do controle alvo. |
| `UNCOVERED` | Auditor | Controle interativo descoberto na árvore sem contrato registrado cobrindo seu `ComponentID`. |
| `MODEL_MISMATCH` | Contrato | A interação na UI não produziu a mutação esperada no `AuthoringModel` ou `AuditionTransport`. |
| `UNDO_MISMATCH` | Contrato | `undo()` ou `redo()` não reverteu/reaplicou a mutação com fidelidade. |
| `VIEW_MISMATCH` | Contrato | A view não refletiu o estado esperado após a interação ou *refresh*. |
| `AUDIO_MISMATCH` | Contrato | A medição PCM do `HeadlessAudioProbe` (RMS, pan L/R ou Goertzel) divergiu do efeito acústico esperado. |
| `DIALOG_TIMEOUT` | Contrato | Um diálogo, `FileChooser` ou `PopupMenu` não foi interceptado pelo `UiTestDialogSeam`. |

---

## 4. Como Adicionar um Novo Controle à UI do Studio

1. **Atribua um `ComponentID` determinístico** no construtor do componente (`control.setComponentID("dominio.nome_do_controle")`). Nunca deixe botões, sliders, combos ou editores interativos sem `ComponentID`, caso contrário o `UiControlAuditor` falhará com `MISSING_ID`.
2. **Para widgets com `paint()` / `mouseDown()` customizados**, implemente `ITestableCustomControl` (`studio/Source/Testing/UiTestHooks.h`) e retorne os sub-alvos clicáveis/arrastáveis em `get_testable_targets()`.
3. **Para menus ou diálogos modais**, utilize `UiTestDialogSeam::show_popup_menu`, `UiTestDialogSeam::show_modal_menu`, `UiTestDialogSeam::intercept_file_chooser` ou `UiTestDialogSeam::intercept_alert`.
4. **Registre a cobertura e a asserção** em um dos arquivos `studio/Source/Testing/UiControlContracts*.cpp` adicionando o `ComponentID` (ou padrão glob) em `coveredPatterns` e exercitando-o via `UiInteractionDriver`, caso contrário o auditor acusará `UNCOVERED`.
