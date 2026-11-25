# Spec Delta

## ADDED Requirements

### Requirement: Docks de barra com grip lateral tátil e sem cabeçalho padrão de slot
O Studio SHALL disponibilizar docks de barra de ferramentas (`Project Toolbar` e `Transport Bar`) com altura compacta unificada e um grip lateral tátil na margem esquerda, sem renderizar o cabeçalho de slot tradicional de 20px (`SlotHeaderBar`). O grip lateral SHALL permitir arrastar a barra para reordenamento no topo ou para destacá-la como janela flutuante independente.

#### Scenario: Arraste da barra pelo grip lateral para reposicionamento
- **WHEN** o usuário clica e arrasta o grip lateral tátil de uma barra de ferramentas posicionada no topo
- **THEN** o Studio inicia o gesto de docking com retângulo translúcido de prévia de soltura
- **AND** soltar sobre o topo reposiciona a barra na divisão desejada

#### Scenario: Ausência de cabeçalho tradicional SlotHeaderBar sobre a barra
- **WHEN** uma barra de ferramentas com grip lateral está encaixada no topo da área útil
- **THEN** o Studio não desenha a barra de 20px com abas e botões de ação sobre o componente
- **AND** a barra utiliza sua altura útil contínua de 32px a 34px

### Requirement: Alternância entre modos minimizado e maximizado via grip lateral
O Studio SHALL permitir alternar os docks de barra entre o modo Minimizado (Compacto) e o modo Maximizado (Completo) por duplo-clique no grip lateral ou via menu contextual acionado com o botão direito do mouse. O modo minimizado do transporte SHALL ocultar sliders de parâmetros e status extenso, exibindo apenas controles básicos de reprodução e o timecode.

#### Scenario: Duplo-clique no grip lateral alterna para modo compacto
- **WHEN** o usuário executa um duplo-clique sobre a área do grip lateral da `Transport Bar` maximizada
- **THEN** a barra recolhe o viewport dinâmico de sliders e rótulos secundários
- **AND** a barra permanece visível no topo ocupando largura reduzida com botões essenciais e timecode

#### Scenario: Menu de contexto no grip lateral com botão direito
- **WHEN** o usuário clica com o botão direito do mouse sobre o grip lateral de uma barra de ferramentas
- **THEN** o Studio exibe um menu popup com opções para alternar entre modo compacto e expandido e para alternar entre janela flutuante e encaixe no topo

### Requirement: Persistência permanente dos docks de barra sem opção de fechar
Os docks de barra de ferramentas (`Project Toolbar` e `Transport Bar`) SHALL ser permanentes e não exibir botão de fechar ('X'). Quando destacados em janela flutuante, a janela SHALL não exibir botão de fechar do sistema operacional e MUST fornecer uma ação de `Dock Back` para retornar ao topo da interface principal.

#### Scenario: Impossibilidade de fechar o dock de transporte
- **WHEN** o usuário inspeciona a barra de ferramentas no topo ou seus menus de contexto
- **THEN** nenhum botão de fechar ('X') ou comando de fechamento definitivo é disponibilizado para a barra
- **AND** o atalho `[Trn]` na `WorkspaceBar` alterna o foco ou estado da barra em vez de ocultá-la

#### Scenario: Janela flutuante com ação de retorno Dock Back
- **WHEN** a barra de transporte é destacada como janela flutuante independente
- **THEN** a janela flutuante é exibida sem botão de fechar nativo do sistema operacional
- **AND** acionar a ação de `Dock Back` na janela flutuante reintegra a barra ao slot superior da janela principal

### Requirement: Controles com símbolos vetoriais e cores semânticas ativas
Os botões das barras de ferramentas de projeto e de transporte SHALL renderizar símbolos vetoriais nativos em vez de texto, com dimensões compactas e feedback visual semântico de estado ativo para reprodução (Play verde), pausa (Pause âmbar) e repetição (Loop ciano).

#### Scenario: Renderização vetorial dos controles de transporte
- **WHEN** a `Transport Bar` é renderizada na tela
- **THEN** os botões `Play`, `Pause`, `Stop`, `Loop` e `Key Off` exibem glifos vetoriais geométricos nítidos em vez de strings de texto
- **AND** cada botão ocupa largura compacta de 28px a 32px

#### Scenario: Feedback de cor semântica no estado ativo
- **WHEN** o evento ativo entra em reprodução (`is_playing() == true`)
- **THEN** o botão vetorial `Play` exibe o fundo em verde semântico ativo
- **AND** os demais botões mantêm as cores de superfície padrão do tema

### Requirement: Tooltips globais com atalhos de teclado
O Studio SHALL manter ativa uma janela global de tooltips para exibir descrições e atalhos de teclado nos controles da interface ao passar o cursor do mouse sobre botões com símbolos e ações de projeto.

#### Scenario: Exibição de tooltip com atalho de teclado no botão Play
- **WHEN** o cursor do mouse repousa sobre o botão com símbolo vetorial de `Play`
- **THEN** o Studio exibe uma janela de tooltip flutuante com o texto `Play (Space)`

#### Scenario: Exibição de tooltip com atalho no botão Save Project
- **WHEN** o cursor do mouse repousa sobre o botão com símbolo vetorial de `Save Project`
- **THEN** o Studio exibe uma janela de tooltip flutuante com o texto `Save Project (Ctrl+S)`
