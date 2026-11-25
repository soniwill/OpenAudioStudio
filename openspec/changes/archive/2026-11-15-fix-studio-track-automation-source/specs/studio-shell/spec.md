# Spec Delta

## MODIFIED Requirements

### Requirement: Edição direta por arraste de pontos na pista de automação da Timeline
A faixa `Volume Automation` da Timeline SHALL exibir e editar a curva temporal de volume da pista em milissegundos e decibéis. O editor SHALL permitir arrastar pontos existentes e consolidar o movimento completo como uma única operação de Undo/Redo. A faixa SHALL permitir inserir pontos por duplo clique e SHALL preservar as demais curvas da pista.

#### Scenario: Arrastar ponto de automação na timeline com Undo/Redo por gesto
- **WHEN** o usuário arrasta um ponto da curva temporal de volume e solta o botão do mouse
- **THEN** o tempo e o valor do ponto são atualizados ao vivo durante o arraste e registrados como um único passo de Undo/Redo ao final do gesto

#### Scenario: Editar a curva temporal de volume em uma pista com outras automações
- **WHEN** uma pista contém uma curva temporal de volume e outras curvas, em qualquer ordem, e o usuário abre a faixa pelo botão `A`
- **THEN** a faixa exibe apenas os pontos da curva temporal de volume
- **AND** selecionar, arrastar ou inserir pontos na faixa preserva as demais curvas, inclusive as curvas por parâmetro exibidas no Inspector

#### Scenario: Criar a curva temporal de volume em uma pista que só tem outras automações
- **WHEN** o usuário abre a faixa de uma pista que tem outras automações e ainda não tem curva temporal de volume
- **THEN** a faixa exibe a grade vazia e a abertura preserva os dados da pista
- **WHEN** o usuário dá duplo clique na grade
- **THEN** a faixa cria uma curva temporal de volume com o ponto inserido nas coordenadas correspondentes ao gesto
- **AND** as curvas existentes mantêm seus pontos, origem, alvo e ordem
