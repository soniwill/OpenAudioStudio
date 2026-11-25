# Tasks

## 1. Helpers Vetoriais e Ativação Global de Tooltips

- [x] 1.1 Criar utilitário vetorial com `juce::Path` (`studio/Source/UI/VectorIcons.h` e `.cpp` ou extensão de `OaeDarkLookAndFeel`) para desenhar os glifos geométricos de reprodução (`Play`, `Pause`, `Stop`, `Loop`, `Key Off`) e de projeto (`New`, `Open`, `Save`, `Build Bank`, `Help`), com suporte aos tokens semânticos de cores ativas de `Theme.h`. Verificar compilação na camada Studio.
- [x] 1.2 Instanciar `juce::TooltipWindow` no `MainComponent`, estilizá-la conforme `OaeDarkLookAndFeel` e configurar tooltips com nomes e atalhos de teclado (`Play (Space)`, `Save Project (Ctrl+S)`, `Build Bank (F7)`) nos botões vetoriais. Verificar que as tooltips surgem ao passar o cursor do mouse.

## 2. GripHandle e Arquitetura de Docks de Barra de Ferramentas

- [x] 2.1 Criar o componente `GripHandle` (~12px de largura) com textura pontilhada tátil (`::`), cursor `DraggingHandCursor`, disparo de drag & drop para docking, duplo-clique para alternar entre minimizado/maximizado e clique com botão direito para menu de contexto (Minimizar/Maximizar, Flutuar/Dock Back). Verificar respostas aos gestos do mouse.
- [x] 2.2 Adaptar `DockingLayoutManager` e `MainComponent` para identificar docks do tipo barra de ferramentas, suprimindo o cabeçalho tradicional de 20px (`SlotHeaderBar`) sobre eles e assegurando a política de persistência permanente (`can_close = false`).
- [x] 2.3 Adaptar a janela flutuante (`OaeFloatingPanelWindow`) quando hospeda docks de toolbar para usar proporções compactas de barra e exibir um botão dedicado de `Dock Back` (sem botão nativo de fechar 'X'). Verificar retorno ao topo ao acionar o comando.

## 3. Separação de ProjectToolbar e Refatoração de TransportBar

- [x] 3.1 Implementar `ProjectToolbar` (`studio/Source/UI/ProjectToolbar.h` e `.cpp`) como dock modular contendo o grip lateral e os botões vetoriais de ações de projeto (`New`, `Open`, `Save`, `Build Bank`, `Help`). Conectar callbacks aos métodos correspondentes de `MainComponent`.
- [x] 3.2 Refatorar `TransportBar` (`studio/Source/UI/TransportBar.h` e `.cpp`) para focar exclusivamente nos controles de reprodução, timecode, status de áudio e viewport dinâmico de sliders de parâmetros, integrando o grip lateral e implementando a alternância entre o modo Maximizado (completo) e o modo Minimizado (apenas botões de transporte essenciais e timecode).
- [x] 3.3 Atualizar a inicialização do layout superior em `MainComponent`, posicionando `ProjectToolbar` e `TransportBar` lado a lado no slot `Top` por padrão, atualizando o botão rápido `[Trn]` da `WorkspaceBar` para alternar foco/minimização em vez de fechar, e tratando migração de layouts JSON antigos.

## 4. Documentação e Gates de Validação

- [x] 4.1 Atualizar `docs/UI_GUIDELINES.md` e o manual offline (`docs/help/`) documentando a arquitetura dos docks de barra com grip lateral, botões vetoriais, atalhos de teclado e persistência permanente sem botão de fechar.
- [x] 4.2 Executar os gates do projeto (`scripts/verify.ps1 -Layer studio`, `scripts/verify.ps1 -Layer docs` e validação estrita do OpenSpec) e validar o comportamento visual do novo topo via MCP (`studio_ui_inspect` e `studio_ui_screenshot`).

## Workflow follow-up

- Arquivar a change com `openspec-archive-change` após implementação e validação completa.
- Sincronizar os deltas com `openspec/specs/studio-shell/spec.md`.
