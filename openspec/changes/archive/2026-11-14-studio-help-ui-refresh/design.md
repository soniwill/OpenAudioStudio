# Design: studio-help-ui-refresh

## Context

O OpenAudio Studio recebeu em 14/11/2026 um conjunto de melhorias ergonômicas e funcionais: a Workspace Bar superior com seleção de templates e presets de layout, docking aprimorado estilo Unity 6 com cabeçalhos grafite, navegação hierárquica por breadcrumbs na timeline, swatches de cores de pista, waveforms poligonais recursivos e agrupamento em cards no Inspector com suporte a Shift para ajuste fino e duplo-clique de reset.

Os capítulos 1, 5 e 6 do manual ainda continham descrições anteriores a essas mudanças, e as imagens em `docs/manual/assets/img/` retratavam a interface prévia de 12/11/2026.

## Goals / Non-Goals

**Goals:**
- Regenerar via MCP (`studio_ui_screenshot`) as capturas de tela principais para retratar com precisão o estado atual da interface:
  - `studio-overview.png` (janela inteira com a Workspace Bar)
  - `studio-workspace-bar.png` (detalhe da barra de templates, presets e docking)
  - `studio-timeline.png` (timeline com breadcrumb, cores de pista e waveforms poligonais)
  - `studio-inspector.png` (inspector com cards visuais e sliders)
  - `studio-transport.png` (barra de transporte vetorial)
  - `studio-tree.png` (navegador com árvore atualizada)
- Atualizar a redação dos capítulos correspondentes:
  - `01-visao-geral-interface.html`: explicar as ferramentas da Workspace Bar, os templates `Default (Authoring)`, `Stage Focus`, `Mixing & Mastering`, salvamento e exclusão de presets, e as regras de docking do Unity 6.
  - `05-timeline-e-waveforms.html`: explicar o uso do breadcrumb de navegação pai->filho, atribuição de cores de pista e identificação de Event Instruments poligonais com badge `[EVT]`.
  - `06-inspetor-e-modulacao.html`: explicar o novo layout em cards e os atalhos de slider (arraste 1:1, precisão com Shift e reset por duplo-clique).

**Non-Goals:**
- Não reescrever outros capítulos do manual que já foram atualizados nas fases anteriores.
- Não modificar o código C++ do Studio.

## Decisions

### 1. Captura direta via servidor MCP do Studio
- **Decisão:** Usar `studio_ui_screenshot` com `component_id` específicos (`workspace_bar`, `timeline_view`, `inspector_panel`, `transport_bar`, `browser_panel`) e `root` para gerar os PNGs oficiais.
- **Racional:** Garante renderização pixel-perfect, temas oficiais idênticos aos do binário em execução e processo 100% determinístico e automatizado.

### 2. Integração em Capítulos Existentes com Callouts Visuais
- **Decisão:** Manter a numeração e organização dos Capítulos 1, 5 e 6, inserindo caixas semânticas (`theory-box`, `tip-box`, `purpose-box`) para destacar as novidades de ergonomia.
- **Racional:** Preserva a coerência de leitura e o mapa do manual existente, garantindo que o usuário encontre as novidades no ponto exato onde a funcionalidade é abordada.

## Risks / Trade-offs

- **[Risco]** Diferença visual em telas pequenas caso as capturas usem resoluções muito amplas.
  - *Mitigação:* O CSS do manual (`manual.css`) já possui regras responsivas automáticas (`max-width: 100%; height: auto; border-radius: var(--radius)`).
