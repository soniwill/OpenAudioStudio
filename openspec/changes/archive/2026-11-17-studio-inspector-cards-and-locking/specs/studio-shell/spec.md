# Spec Delta: studio-shell

## ADDED Requirements

### Requirement: Bloqueio de seleção (Lock Inspector) no cabeçalho do InspectorPanel
O `InspectorPanel` SHALL fornecer um botão de alternância com ícone de cadeado em sua barra superior para travar a inspeção no item atualmente ativo, ignorando mudanças externas de seleção na Timeline, navegador ou mixer enquanto estiver bloqueado.

#### Scenario: Ativação do Lock congela a visualização do item inspecionado
- **WHEN** o usuário seleciona um item (evento, pista, clipe ou barramento) e ativa o botão de cadeado (Lock) no cabeçalho do `InspectorPanel`
- **THEN** o botão passa para o estado ativo com destaque visual
- **AND** seleções subsequentes na Timeline ou no navegador de projeto não alteram o conteúdo exibido no `InspectorPanel`

#### Scenario: Desativação do Lock sincroniza imediatamente com a seleção atual
- **WHEN** o usuário desativa o botão de cadeado com uma seleção diferente ativa no projeto
- **THEN** o `InspectorPanel` atualiza imediatamente sua visualização para refletir o item selecionado no momento

### Requirement: Ações de contexto e padronização de cabeçalhos nos cards do InspectorPanel
Cada card de inspeção no `InspectorPanel` SHALL possuir um cabeçalho padronizado contendo chevron colapsável, badge semântica do tipo inspecionado e botão de menu contextual (`...`) com ações de redefinição e cópia de valores.

#### Scenario: Redefinição de valores de um card via menu contextual
- **WHEN** o usuário clica no menu de ações (`...`) no cabeçalho de um card e aciona a opção "Reset to Default"
- **THEN** os parâmetros gerenciados por aquele card retornam aos seus valores padrão de fábrica
- **AND** a alteração é consolidada em um único passo no histórico de Undo/Redo do `AuthoringModel`

#### Scenario: Cópia e colagem de propriedades entre cartões compatíveis
- **WHEN** o usuário aciona "Copy Values" no menu de um card e posteriormente "Paste Values" no menu de outro card do mesmo tipo
- **THEN** os parâmetros compatíveis são aplicados ao alvo em uma transação atômica de Undo
