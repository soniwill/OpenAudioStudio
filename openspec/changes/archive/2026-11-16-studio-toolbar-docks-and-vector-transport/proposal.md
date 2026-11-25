# Proposal

## Why

Atualmente, o OpenAudio Studio concentra os comandos de projeto (`New`, `Open`, `Save`, `Build Bank`, `Help`) e os controles de audição e reprodução (`Play`, `Pause`, `Stop`, `Loop`, `Key Off`, `Timecode`, `Status` e sliders dinâmicos de parâmetros) em uma única classe volumosa (`TransportBar`), consumindo mais de 650px horizontais exclusivamente com botões de texto longo (`juce::TextButton`). Além disso, o sistema de docking impõe um cabeçalho tradicional de 20px (`SlotHeaderBar`) sobre a barra, gerando desperdício vertical desnecessário.

Quando o usuário destaca a barra de transporte em uma janela flutuante ou a oculta, ele perde o acesso imediato a comandos essenciais de projeto como Salvar e Compilar Bancos. Por fim, o fechamento acidental da barra de transporte remove o controle crítico de áudio da tela, e as tooltips da interface estão silenciosamente inativas devido à ausência de uma instância de `juce::TooltipWindow`. Esta change moderniza o topo do Studio com barras modulares, grip lateral compacto, símbolos vetoriais, tooltips com atalhos e garantia de persistência permanente ("nunca fechar").

## What Changes

- **Separação de Docks**: Desacoplamento da `TransportBar` em dois docks independentes:
  - `Project Toolbar`: Ações de gerenciamento de projeto (`New`, `Open`, `Save`, `Build Bank`, `Help`).
  - `Transport Bar`: Controles de audição e reprodução em tempo real (`Play`, `Pause`, `Stop`, `Loop`, `Key Off`, `Timecode`, `Status` de áudio e viewport dinâmico de sliders de parâmetros).
  - Ambos os docks iniciam posicionados lado a lado dividindo a faixa superior do Studio logo abaixo da `WorkspaceBar`.
- **Grip Lateral (Drag Handle) e Altura Unificada**:
  - Remoção do cabeçalho tradicional de 20px (`SlotHeaderBar`) sobre esses docks de barra.
  - Inclusão de um grip lateral fino (~10px a 14px) na borda esquerda com textura tátil (`::`), permitindo arrasto direto para reordenar ou destacar para janela flutuante.
- **Interação no Grip (Minimizar / Maximizar)**:
  - Duplo-clique no grip lateral alterna instantaneamente entre o modo **Minimizado (Compacto)** e o modo **Maximizado (Completo)**.
  - Clique com o botão direito no grip exibe menu contextual com opções de Minimizar/Maximizar e Destacar/Re-encaixar no Topo.
  - O modo minimizado encolhe a barra (no transporte, mantendo apenas `Play`, `Pause`, `Stop`, `Loop` e `Timecode`, ocultando sliders e status extenso).
- **Invariante de Persistência Permanente ("Nunca Fechar")**:
  - Docks de barra de ferramentas não possuem botão de fechar ('X') e não podem ser destruídos ou ocultados da interface.
  - Quando destacados em janela flutuante, a janela não possui o botão fechar 'X' nativo, exibindo um botão de **"Dock Back"** (Re-encaixar no Topo).
  - O atalho `[Trn]` na `WorkspaceBar` alterna foco ou estado minimizado/maximizado em vez de fechar o painel.
- **Símbolos Vetoriais e Cores Semânticas**:
  - Substituição do texto puro por ícones vetoriais desenhados via `juce::Path` (Play ▶, Pause ❚❚, Stop ■, Loop 🔁, Key Off ⤉, New, Open, Save, Build Bank, Help).
  - Aplicação de cores semânticas ativas (`Theme.h`) para indicar estado (Play verde, Pause âmbar, Loop ciano).
- **Ativação Global de Tooltips**:
  - Instanciação de `juce::TooltipWindow` no `MainComponent`, ativando tooltips em toda a aplicação.
  - Configuração de tooltips ricas com atalhos de teclado nos botões com símbolos (`Play (Space)`, `Save Project (Ctrl+S)`, `Build Bank (F7)`).

## Capabilities

### Modified Capabilities
- `studio-shell`: Adição de requisitos para o padrão de docks de toolbar com grip lateral, persistência permanente sem fechamento, separação de docks de projeto e transporte, botões com símbolos vetoriais e ativação de tooltips globais com atalhos de teclado.

## Impact

- **Código C++ (Studio UI)**:
  - `studio/Source/UI/TransportBar.h` e `.cpp`: Refatoração para comportar apenas controles de áudio, renderização vetorial dos símbolos, supressão de `SlotHeaderBar` e suporte ao modo compacto/minimizado.
  - `studio/Source/UI/ProjectToolbar.h` e `.cpp`: Criação do novo componente modular de barra de ferramentas de projeto.
  - `studio/Source/UI/ToolbarDockComponent.h` e `.cpp` (ou especialização em `DockingLayoutManager`): Implementação do grip lateral tátil, menus de contexto, duplo-clique de minimização e política de não-fechamento (`canClose = false`).
  - `studio/Source/UI/DockingLayoutManager.h` e `.cpp`: Suporte a janela flutuante sem botão fechar nativo (com ação de dock-back dedicada) e posicionamento lado a lado de toolbars no slot superior.
  - `studio/Source/MainComponent.h` e `.cpp`: Instanciação de `juce::TooltipWindow`, instanciação de `ProjectToolbar`, amarração de comandos e inicialização do layout padrão no topo.
  - `studio/Source/UI/OaeDarkLookAndFeel.h` e `.cpp`: Helpers vetoriais para ícones e estilização de tooltips.
- **Documentação e Manuais**:
  - Atualização do manual offline do Studio (`docs/help/`) e de `docs/UI_GUIDELINES.md` documentando o novo layout do topo, os ícones, os atalhos e a ergonomia de grip lateral.
