# Spec Delta

## ADDED Requirements

### Requirement: Encerramento normal do Studio com banco de prévia carregado

O Studio SHALL encerrar normalmente quando o usuário fechar a janela com um projeto aberto e um banco de prévia carregado. O processo SHALL concluir a liberação dos recursos da sessão e terminar com código de saída zero.

#### Scenario: Fechar o Studio com o projeto de exemplo aberto

- **WHEN** o usuário abre o projeto de exemplo, aguarda a confirmação de que a prévia está pronta e fecha a janela principal
- **THEN** o processo MUST terminar com código de saída zero
- **AND** ao abrir o mesmo projeto em uma nova sessão, a prévia MUST voltar a ficar disponível.
