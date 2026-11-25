# Tasks

## 1. Modelo de Máscara e Tradução Bidirecional (Studio Model)

- [x] 1.1 [Studio Model] Criar helper de tradução bidirecional entre Máscara de Camada (`param_name`, `min_norm`, `max_norm`, `fade_norm`, detecção de curva personalizada) e `AutomationCurve` (`target_property = "volume"`) com transação atômica de Undo/Redo em `AuthoringModel`; verificar por teste unitário/smoke que criar/alterar a máscara gera pontos válidos e reconhece curvas manuais complexas sem sobrescrevê-las.
- [x] 1.2 [Studio Model] Garantir helper/fluxo para criação rápida de camada de stem em loop (`add_stem_layer` / `import_audio_stem_layer` com `loop = true`) em passo único de Undo/Redo; verificar que desfazer/refazer restaura o estado anterior de faixas, clipes e samples do projeto.

## 2. Interface `LayerStackView` e Seletor no `TimelinePanel` (Studio UI)

- [x] 2.1 [Studio UI] Implementar `studio/Source/UI/LayerStackView.h` e `LayerStackView.cpp` seguindo os tokens de `Theme.h` e a referência do Figma (`OpenAudioStudio — Stage Layers View`), registrando os novos arquivos em `studio/CMakeLists.txt` e compilando o target `oae_studio` em Release sem erros.
- [x] 2.2 [Studio UI] Implementar o card bipartido de camada (`StemLayerCard`) com coluna esquerda fixa (`350 px`: barra de cor da pista, botão Olho/Mute, botão `S` Solo, nome do clipe, badge de quantização/sync, waveform via `WaveformCache` e slider de Opacidade `0%–100%` mapeado para `volume_db`) e coluna direita (`MaskRangeCanvas` com seletor de parâmetro, barra `[min .. max]` com rampa de fade, badge de status em tempo real e linha vertical do cursor ao vivo alinhada entre todas as camadas); verificar seleção sincronizada com o `InspectorPanel`.
- [x] 2.3 [Studio UI] Adicionar suporte na `LayerStackView` à visualização opcional de camada de ajuste (`Adjustment Layer`) condicionada por parâmetro e ao drag-and-drop de arquivos de áudio (`.wav`, `.ogg`, `.mp3`, `.flac`) criando faixa de stem em loop; verificar importação e Undo/Redo em teste de smoke.
- [x] 2.4 [Studio UI] Integrar o seletor segmentado `[ Timeline | Layers ]` e os botões de ação do modo `Layers` (`+ Stem Layer`) na barra superior do `TimelinePanel`, mantendo `Timeline` como modo padrão e `TimelineParameterStrip` visível em ambos os modos; verificar compilação Release e ausência de regressão nos smokes existentes de `TimelinePanel`.

## 3. Documentação, Evidência Visual MCP e Gates de Verificação

- [x] 3.1 [Docs] Documentar o modo `Layers` (`LayerStackView`), máscaras visuais por parâmetro, Opacidade, Olho/Solo e alternância reversível com a `Timeline` no manual offline em `studio/Resources/help/`, conferindo integridade de links relativos via `scripts/verify.ps1 -Layer docs`.
- [x] 3.2 [Studio Tests] Adicionar cenário de smoke automatizado no Studio cobrindo alternância `Timeline -> Layers -> Timeline`, ajuste de opacidade/mute/solo, geração de máscara de parâmetro com Undo/Redo e importação de stem em loop; executar `./scripts/verify.ps1 -Layer studio` (Release) confirmando aprovação integral.
- [x] 3.3 [MCP Verification] Abrir o Studio compilado em Release com `--mcp` sobre cópia temporária do projeto de exemplo, alternar para o modo `Layers`, ajustar parâmetro ao vivo, capturar evidências visuais com `studio_ui_screenshot` / `studio_ui_inspect` via `scripts/mcp_call.js` e confirmar áudio/estado sem erros.
- [x] 3.4 [Gates] Executar `openspec validate "studio-stage-layers-view" --strict` e `./scripts/verify.ps1 -Layer docs` confirmando aprovação de todos os gates antes da implementação/arquivamento.

## Workflow follow-up

- Implementar as tarefas acima somente após autorização explícita usando `openspec-apply-change`.
- Após validação completa dos gates, sincronizar o delta em `openspec/specs/studio-shell/spec.md`, atualizar `docs/ai/memory.md` e arquivar a change com `openspec-archive-change`.
