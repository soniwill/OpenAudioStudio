# Proposal: studio-help-ui-refresh

## Why

Diversas capturas visuais da documentação oficial do OpenAudio Studio em `docs/manual/assets/img/` ainda datavam de 12/11/2026, antes das melhorias substanciais de interface e usabilidade implementadas em 14/11/2026. Além disso, as novidades fundamentais de layout — a Workspace Bar com gerenciamento de templates e presets, o sistema de docking estilo Unity 6 com cabeçalhos grafite, a navegação por breadcrumbs na timeline, as cores de pista, os waveforms poligonais e a apresentação em cards do Inspector com sliders 1:1 e duplo-clique — ainda não estavam devidamente documentadas nos capítulos operacionais correspondentes (Capítulos 1, 5 e 6).

Esta change sincroniza o manual e suas evidências visuais com o estado real do Studio, regenerando todas as capturas através da ferramenta MCP `studio_ui_screenshot` e atualizando as instruções e conceitos da interface.

## What Changes

- **Regeneração de Evidências Visuais via MCP:**
  - `docs/manual/assets/img/studio-overview.png`: Janela completa exibindo a nova barra superior de workspace (`workspace_bar`), docking moderno e cabeçalhos grafite.
  - `docs/manual/assets/img/studio-workspace-bar.png`: Nova captura de detalhe focada na Workspace Bar (templates, presets, botões de visibilidade e Hide All).
  - `docs/manual/assets/img/studio-timeline.png`: Timeline atualizada exibindo breadcrumbs de navegação, swatches de cores de pista, waveforms poligonais e curvas Bézier.
  - `docs/manual/assets/img/studio-inspector.png`: Inspector atualizado com o layout em cards semânticos.
  - `docs/manual/assets/img/studio-transport.png`: Barra de transporte com os controles vetoriais refinados.
  - `docs/manual/assets/img/studio-tree.png`: Navegador com ícones e opções atualizadas.
- **Atualização Editorial dos Capítulos Operacionais:**
  - **Capítulo 1 (`01-visao-geral-interface.html`):** Documentação completa da Workspace Bar (templates `Default`, `Stage Focus`, `Mixing & Mastering`; presets salvos; `Reset Layout`; botão `Hide All / Restore`), além das regras de docking do Unity 6 e cabeçalhos grafite.
  - **Capítulo 5 (`05-timeline-e-waveforms.html`):** Documentação do sistema de breadcrumbs para navegação em eventos aninhados, cores customizáveis de pista, renderização de waveforms poligonais com badge `[EVT]` e curvas Bézier ribbon.
  - **Capítulo 6 (`06-inspetor-e-modulacao.html`):** Documentação do layout em cards visuais, sliders 1:1, controle de precisão fina com <kbd>Shift</kbd> e duplo-clique para reset.

## Capabilities

### New Capabilities
*(Nenhuma nova capacidade introduzida).*

### Modified Capabilities
- `studio-shell`: Atualiza os requisitos do manual com a documentação da Workspace Bar, presets de workspace, docking avançado, breadcrumbs de timeline e layout em cards do Inspector.

## Impact

- **Documentação e Ativos:** Atualização de 3 arquivos HTML em `docs/manual/chapters/` e substituição/adição de imagens PNG em `docs/manual/assets/img/`.
- **Camadas do Repositório:** Afeta exclusivamente a camada `docs` (`scripts/verify.ps1 -Layer docs`).
- **Código e Binários:** Zero impacto em C++, C# ou binários do Studio/Unity.
