# Tasks: studio-help-ui-refresh

## 1. Regeneração de Capturas Nativas via MCP

- [x] 1.1 Capturar a janela inteira e os componentes específicos (`workspace_bar`, `timeline_view`, `inspector_panel`, `transport_bar`, `browser_panel`) usando `studio_ui_screenshot` conectado ao Studio com `example_project/project.json`, salvando em `docs/manual/assets/img/` (`studio-overview.png`, `studio-workspace-bar.png`, `studio-timeline.png`, `studio-inspector.png`, `studio-transport.png` e `studio-tree.png`) e verificando a legibilidade.

## 2. Atualização do Capítulo 1: Visão Geral da Interface e Workspace Bar

- [x] 2.1 Atualizar `docs/manual/chapters/01-visao-geral-interface.html` documentando a Workspace Bar, os templates nativos `Default (Authoring)`, `Stage Focus` e `Mixing & Mastering`, salvamento e exclusão de presets de layout personalizados, o botão `Reset Layout`, o botão `Hide All / Restore` e o modelo de docking Unity 6 com cabeçalhos grafite, integrando as novas capturas.

## 3. Atualização do Capítulo 5: Timeline, Breadcrumbs e Waveforms Poligonais

- [x] 3.1 Atualizar `docs/manual/chapters/05-timeline-e-waveforms.html` documentando a navegação por breadcrumbs para sub-eventos, atribuição de cores de pista nos swatches de cabeçalho, waveforms poligonais recursivos em Event Instruments (`[EVT]`) e curvas de automação Bézier ribbon, integrando a nova captura da timeline.

## 4. Atualização do Capítulo 6: Inspetor em Cards e Sliders 1:1

- [x] 4.1 Atualizar `docs/manual/chapters/06-inspetor-e-modulacao.html` documentando a organização visual em cards modulares, os sliders 1:1 com precisão fina via tecla Shift (0.1 dB/st) e restauração do valor padrão por duplo-clique com registro no histórico de Undo, integrando a nova captura do Inspector.

## 5. Validação e Gates de Documentação

- [x] 5.1 Executar a validação estrita da camada de documentação através de `scripts/verify.ps1 -Layer docs` (cobrindo `openspec validate --all --strict`, verificação de integridade de links markdown locais e conferência de limite de 100 linhas em `docs/ai/memory.md`), garantindo aprovação com código de saída zero.
