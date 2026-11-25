# Spec Delta

## ADDED Requirements

### Requirement: Exclusão de pontos na faixa de automação da Timeline

A faixa `Volume Automation` SHALL permitir excluir um ponto individual por `Delete`/`Backspace` com a faixa focada e pelo menu de contexto do ponto. Excluir o último ponto SHALL remover a curva temporal de volume da pista, devolver a faixa à grade vazia e preservar as demais curvas da pista. Cada exclusão SHALL ocupar um único passo de Undo/Redo.

#### Scenario: Excluir um ponto preservando as demais curvas

- **WHEN** a faixa `Volume Automation` está focada, um ponto está selecionado e a pista contém outras automações
- **THEN** `Delete` ou `Backspace` remove apenas o ponto selecionado
- **AND** as demais curvas mantêm origem, alvo, ordem e pontos
- **AND** um único `Ctrl+Z` restaura o ponto removido e um `Ctrl+Y` o exclui novamente

#### Scenario: Excluir o último ponto remove a curva temporal

- **WHEN** a curva temporal de volume da pista tem um único ponto e o usuário o exclui pela faixa
- **THEN** a curva temporal de volume é removida da pista
- **AND** a faixa volta a exibir a grade vazia
- **AND** as outras automações da pista permanecem inalteradas e na mesma ordem
- **AND** um único `Ctrl+Z` restaura a curva com o ponto original

#### Scenario: Excluir um ponto pelo menu de contexto

- **WHEN** o usuário clica com o botão direito sobre um ponto da faixa e escolhe a opção de exclusão
- **THEN** o ponto é removido com o mesmo resultado e o mesmo passo único de Undo/Redo da exclusão por teclado

#### Scenario: Tecla de exclusão sem ponto selecionado

- **WHEN** a faixa `Volume Automation` está focada e não há ponto selecionado nem sob o ponteiro
- **THEN** `Delete` ou `Backspace` não altera pontos, curvas ou clipes

### Requirement: Roteamento da tecla de exclusão entre clipe e faixa de automação

O foco de teclado da Timeline SHALL seguir o último alvo clicado na pista, seja um clipe ou a faixa de automação. Com a faixa focada, `Delete`/`Backspace` SHALL atuar somente sobre a automação; com um clipe como último alvo clicado, a tecla SHALL remover o clipe selecionado como antes.

#### Scenario: Clique no ponto seguido da tecla de exclusão

- **WHEN** o usuário clica em um ponto da faixa e pressiona `Delete`
- **THEN** o ponto é removido
- **AND** nenhum clipe ou item lógico da timeline é removido

#### Scenario: Clique no clipe seguido da tecla de exclusão

- **WHEN** o usuário clica em um clipe depois de interagir com a faixa de automação e pressiona `Delete`
- **THEN** o clipe selecionado é removido como antes
- **AND** os pontos e curvas de automação da pista permanecem inalterados
