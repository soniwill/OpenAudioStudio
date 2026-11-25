# Design

## Context

Consulte `proposal.md` para a motivação da separação e modernização dos docks de barra.

Atualmente:
1. `TransportBar` (`studio/Source/UI/TransportBar.cpp`) acumula ações de projeto (`New`, `Open`, `Save`, `Build Bank`, `Help`) e audição (`Play`, `Pause`, `Stop`, `Loop`, `Key Off`, timecode, status e sliders dinâmicos de parâmetros).
2. O gerenciador de docking (`DockingLayoutManager`) trata a `TransportBar` como um painel comum, sobrepondo uma `SlotHeaderBar` de 20px de altura com abas e botões `<>`, `^`, `X`.
3. Todos os botões usam `juce::TextButton` com rótulos de texto estáticos, ocupando mais de 650px horizontais.
4. Nenhuma `juce::TooltipWindow` está instanciada na aplicação, tornando as tooltips configuradas com `setTooltip()` inativas em tempo de execução.

## Goals / Non-Goals

**Goals:**
- Separar comandos de projeto e de áudio em dois componentes modulares: `ProjectToolbar` e `TransportBar`.
- Integrar um grip lateral tátil (`GripHandle`, ~12px) na borda esquerda de cada barra para arrasto de docking, eliminando o cabeçalho superior de 20px (`SlotHeaderBar`) sobre esses componentes.
- Implementar duplo-clique no grip para alternar entre modo minimizado (compacto) e maximizado (expandido), além de menu de contexto ao clicar com botão direito.
- Tornar esses docks permanentes: sem botão 'X', impossíveis de fechar/ocultar por engano; quando destacados em janela flutuante, prover botão de ação dedicado `Dock Back`.
- Substituir textos dos botões por símbolos vetoriais nativos desenhados com `juce::Path` e feedback de cores semânticas ativas (`Theme.h`).
- Instanciar `juce::TooltipWindow` em `MainComponent`, ativando tooltips em toda a aplicação com atalhos de teclado explicados.
- Preservar compatibilidade de presets e arquivos de layout JSON (`oae_studio_layout.json`).

**Non-Goals:**
- Não alterar a arquitetura dos docks convencionais da área de trabalho (Browser, Stage, Inspector, Bottom Deck, Log), que mantêm seus cabeçalhos com abas.
- Não alterar o motor de áudio C/C++, DSP, timeline logic ou contratos de API C.
- Não introduzir arquivos de assets de imagens externas (PNG/SVG) para ícones; todos os glifos devem ser desenhados vetorialmente via código.

## Decisions

### Decisão 1: Barra de Projeto Modular (`ProjectToolbar`) vs. Integração na `WorkspaceBar`
- **Escolha**: Criar um componente de dock independente `ProjectToolbar` que inicia dividindo o slot superior (`DockSlotId::Top`) lado a lado com a `TransportBar`.
- **Alternativa rejeitada**: Incorporar os botões de projeto fixos na `WorkspaceBar`.
- **Justificativa**: Manter os botões de projeto como uma toolbar dockable permite que o usuário posicione as ferramentas onde preferir, destaque a barra para outro monitor ou recolha a barra sem misturar a semântica de gerenciamento de workspace/presets com as ações do arquivo de projeto.

### Decisão 2: Grip Lateral (Drag Handle) vs. Cabeçalho Superior Fino
- **Escolha**: Um componente grip lateral vertical de ~12px na extrema esquerda da própria barra com textura pontilhada tátil (`::`), cursor `DraggingHandCursor` e detecção de gestos (arrasto, duplo-clique, botão direito).
- **Alternativa rejeitada**: Manter um cabeçalho superior tradicional ou uma mini-barra acima dos botões.
- **Justificativa**: A economia de 20px de altura vertical na janela principal é um ganho massivo para a Timeline e para os Decks inferiores. Com botões vetoriais compactos (~30px cada), o espaço horizontal no topo é mais do que suficiente para comportar o grip e os controles.

### Decisão 3: Ícones Vetoriais via `juce::Path` em Helper Centralizado
- **Escolha**: Criar helpers vetoriais utilitários (ex: `VectorIcons::draw_play()`, `draw_stop()`, `draw_save()`, etc.) desenhados diretamente em instâncias de botões com estilo derivado do `OaeDarkLookAndFeel`.
- **Alternativa rejeitada**: Usar fontes de ícones ou imagens PNG/SVG externas em disco.
- **Justificativa**: `juce::Path` é nativo, ultra performático, perfeitamente nítido em qualquer escala High-DPI / Retina e permite coloração dinâmica direta com base nas cores semânticas ativas do tema (`#2b9348` para Play, `#ffffb703` para Pause, `#00b4d8` para Loop).

### Decisão 4: Política de Persistência Permanente e Janela Flutuante Especial
- **Escolha**: Docks do tipo barra são marcados com `can_close = false`. Na janela flutuante (`OaeFloatingPanelWindow`), a barra de transporte é hospedada com dimensões compactas de toolbar e um botão explícito de "Dock Back" em vez do botão 'X' de fechamento do sistema operacional.
- **Alternativa rejeitada**: Permitir fechar e reabrir via atalho no menu ou WorkspaceBar.
- **Justificativa**: Previne o erro humano de fechar o controle de áudio da DAW, garantindo que o transporte esteja sempre imediatamente acessível na tela principal ou em tela secundária.

## Risks / Trade-offs

- **[Migração de Layouts Existentes]**: Layouts antigos em `oae_studio_layout.json` podem não conter `ProjectToolbar`.
  - *Mitigação*: Na desserialização do layout, se o nó do topo contiver apenas `Transport`, o `DockingLayoutManager` divide o slot automaticamente para alocar a `ProjectToolbar` na esquerda e `TransportBar` na direita.
- **[Sensibilidade de Clique no Grip]**: Duplo-clique no grip não deve disparar arrasto acidental.
  - *Mitigação*: Utilizar o threshold padrão de movimento do JUCE (`mouseDrag` só inicia após ultrapassar o raio de arraste), permitindo que `mouseDoubleClick` seja tratado limpidamente.
- **[TooltipWindow e Estilização]**: Janela de tooltip padrão do JUCE pode usar cores genéricas se não estilizada.
  - *Mitigação*: Conectar a `TooltipWindow` ao `OaeDarkLookAndFeel`, garantindo fundo escuro `bgSurfaceRaised`, borda sutil e tipografia idêntica ao restante do Studio.
